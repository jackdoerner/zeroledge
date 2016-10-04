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

results_dir="./results_verify"
working_dir="./benchmark_temp"

entries_file="${working_dir}/entries.txt"
entry_file="${working_dir}/known.txt"
ledger_file="${working_dir}/ledger.txt"
proof_file="${working_dir}/proof.txt"

ofile_full="${results_dir}/benchmark_results_${ledger_size}_${threads}_full.csv"
ofile_inclusion="${results_dir}/benchmark_results_${ledger_size}_${threads}_inclusion.csv"

command_generate="./zlgenerate -o ${proof_file} -e ${entries_file} ${ledger_file}"
command_full="./zlverify -t${threads} ${proof_file}"
command_inclusion="./zlverify -i -k ${entry_file} -t${threads} ${proof_file}"

run_tests() {
    # --------------------------------------------------------------------------
    # Benchmark loop
    # --------------------------------------------------------------------------
    echo "Benchmarking zlverify: ${repeats} samples, ${ledger_size} entries, ${threads} threads";
    # Indicate the command we just run in the csv file
    rm $ofile_full $ofile_inclusion > /dev/null 2>&1

    mkdir $results_dir
    mkdir $working_dir
    
    echo '======' $command_full '======' >> $ofile_full;
    echo '======' $command_inclusion '======' >> $ofile_inclusion;

    # Run the given command [repeats] times
    for (( i = 1; i <= $repeats ; i++ ))
    do
        # percentage completion
        p=$(( $i * 100 / $repeats))
        # indicator of progress
        l=$(seq -s "+" $i | sed 's/[0-9]//g')

        python ./genledger.py -n ${ledger_size} -o ${ledger_file}

        eval ${command_generate} > /dev/null 2>&1

        touch ${entry_file}
        sort -R ${entries_file} | head -n 1 > ${entry_file}

        /usr/bin/time -f "%E,%U,%S" -o ${ofile_full} -a ${command_full} > /dev/null 2>&1
        /usr/bin/time -f "%E,%U,%S" -o ${ofile_inclusion} -a ${command_inclusion} > /dev/null 2>&1


        echo -ne ${l}' ('${p}'%) \r'

    done;

    echo -ne '\n'

    # Convenience seperator for file
    echo '--------------------------' >> $ofile_full
    echo '--------------------------' >> $ofile_inclusion
}

run_tests
