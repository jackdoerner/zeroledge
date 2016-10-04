#!/bin/bash

#CONFIGURE HERE
repeats=30
ledger_size=10000
threads=32

# A POSIX variable
OPTIND=1         # Reset in case getopts has been used previously in the shell.

# Initialize our own variables:
while getopts "r:n:t:" opt; do
    case "$opt" in
    r)
        repeats=$OPTARG
        ;;
    n)  ledger_size=$OPTARG
        ;;
    t)  threads=$OPTARG
        ;;
    esac
done

shift $((OPTIND-1))

[ "$1" = "--" ] && shift

results_dir="./results_generate"
working_dir="./benchmark_temp"

entries_file="${working_dir}/entries.txt"
incremental_file="${working_dir}/incremental.txt"
incremental_discard_file="${working_dir}/incremental_discard.txt"
ledger_file="${working_dir}/ledger.txt"
incremental_ledger_file="${working_dir}/ledger_incremental.txt"
proof_file="${working_dir}/proof.txt"

ofile_standard="${results_dir}/benchmark_results_${ledger_size}_${threads}_standard.csv"
ofile_incremental_save="${results_dir}/benchmark_results_${ledger_size}_${threads}_incremental_save.csv"
ofile_incremental_io="${results_dir}/benchmark_results_${ledger_size}_${threads}_incremental_io.csv"
ofile_incremental_best="${results_dir}/benchmark_results_${ledger_size}_${threads}_incremental_best.csv"
ofile_incremental_worst="${results_dir}/benchmark_results_${ledger_size}_${threads}_incremental_worst.csv"

command_standard="./zlgenerate -o ${proof_file} -e ${entries_file} -t${threads} ${ledger_file}"
command_incremental_save="./zlgenerate -o ${proof_file} -r ${incremental_file} -e ${entries_file} -t${threads} ${ledger_file}"
command_incremental_io="./zlincrementalio -i ${incremental_file} -o ${proof_file} -r ${incremental_discard_file} -e ${entries_file} -t${threads} ${ledger_file}"
command_incremental_best="./zlgenerate -i ${incremental_file} -o ${proof_file} -r ${incremental_discard_file} -e ${entries_file} -t${threads} ${ledger_file}"
command_incremental_worst="./zlgenerate -i ${incremental_file} -o ${proof_file} -r ${incremental_discard_file} -e ${entries_file} -t${threads} ${incremental_ledger_file}"

run_tests() {
    # --------------------------------------------------------------------------
    # Benchmark loop
    # --------------------------------------------------------------------------
    echo "Benchmarking zlgenerate: ${repeats} samples, ${ledger_size} entries, ${threads} threads";
    # Indicate the command we just run in the csv file
    rm $ofile_standard $ofile_incremental_save $ofile_incremental_io $ofile_incremental_best $ofile_incremental_worst > /dev/null 2>&1

    mkdir $results_dir
    mkdir $working_dir
    
    echo '======' $command_standard '======' >> $ofile_standard;
    echo '======' $command_incremental_save '======' >> $ofile_incremental_save;
    echo '======' $command_incremental_io '======' >> $ofile_incremental_io;
    echo '======' $command_incremental_best '======' >> $ofile_incremental_best;
    echo '======' $command_incremental_worst '======' >> $ofile_incremental_worst;

    # Run the given command [repeats] times
    for (( i = 1; i <= $repeats ; i++ ))
    do
        # percentage completion
        p=$(( $i * 100 / $repeats))
        # indicator of progress
        l=$(seq -s "+" $i | sed 's/[0-9]//g')

        python ./genledger.py -n ${ledger_size} -o ${ledger_file} -p ${incremental_ledger_file}

        /usr/bin/time -f "%E,%U,%S" -o ${ofile_standard} -a ${command_standard} > /dev/null 2>&1
        /usr/bin/time -f "%E,%U,%S" -o ${ofile_incremental_save} -a ${command_incremental_save} > /dev/null 2>&1
        /usr/bin/time -f "%E,%U,%S" -o ${ofile_incremental_io} -a ${command_incremental_io} > /dev/null 2>&1
        /usr/bin/time -f "%E,%U,%S" -o ${ofile_incremental_best} -a ${command_incremental_best} > /dev/null 2>&1
        /usr/bin/time -f "%E,%U,%S" -o ${ofile_incremental_worst} -a ${command_incremental_worst} > /dev/null 2>&1


        echo -ne ${l}' ('${p}'%) \r'

    done;

    echo -ne '\n'

    # Convenience seperator for file
    echo '--------------------------' >> $ofile_standard
    echo '--------------------------' >> $ofile_incremental_save
    echo '--------------------------' >> $ofile_incremental_io
    echo '--------------------------' >> $ofile_incremental_best
    echo '--------------------------' >> $ofile_incremental_worst
}

run_tests
