// zlgenerate - a ZeroLedge proof generator implementation
// Copyright (C) 2015 Jack Doerner
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.



#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <ctime>
#include <vector>
#include <unistd.h>
#include <algorithm>
#include <pthread.h>
#include <getopt.h>

#include "zeroledge.h"
#include "zlutil.h"
#include "ledger.h"
#include "lepprocessor.h"
#include "lbpprocessor.h"
#include "dbpprocessor.h"

#define HELP_TEXT "ZeroLedge Proof Generator 1.0\n\
Usage: zlgenerate [\x1b[4mOPTIONS\x1b[0m] [\x1b[4mLEDGER\x1b[0m]\n\
\n\
Options:\n\
  -h \t\tprint this message\n\
  -t \x1b[4mNUMBER\x1b[0m \tuse \x1b[4mNUMBER\x1b[0m threads\n\
  -g \x1b[4mNUMBER\x1b[0m \tprocess \x1b[4mNUMBER\x1b[0m entries at a time\n\
  -v \x1b[4mNUMBER\x1b[0m \trestrict balances and sums to \x1b[4mNUMBER\x1b[0m bits\n\
  -b \x1b[4mPATH\x1b[0m \tread commitment base seeds from \x1b[4mPATH\x1b[0m\n\
  -c \x1b[4mPATH\x1b[0m \tread elliptic curve parameters from \x1b[4mPATH\x1b[0m\n\
  -i \x1b[4mPATH\x1b[0m \tgenerate incremental proof using data from \x1b[4mPATH\x1b[0m\n\
  -e \x1b[4mPATH\x1b[0m \twrite entries to \x1b[4mPATH\x1b[0m\n\
  -r \x1b[4mPATH\x1b[0m \twrite incremental data to \x1b[4mPATH\x1b[0m\n\
  -o \x1b[4mPATH\x1b[0m \twrite proof to \x1b[4mPATH\x1b[0m\n"

using namespace std;

pthread_mutex_t ledger_lock;
pthread_mutex_t proof_lock;
pthread_mutex_t entries_lock;
pthread_mutex_t incr_src_lock;
pthread_mutex_t incr_data_lock;
pthread_mutex_t incr_dst_lock;

class IncrDataRaw {

public:

	int index;
	string identifier, balance;
	string lec_cx, lec_ylsb, lep_r;
	string lep_b1, lep_b2, lep_b3;
	string lep_gamma_cx, lep_gamma_ylsb;
	vector<string> lbc_cx;
	vector<string> lbc_ylsb;
	vector<string> lbp_gamma_cx;
	vector<string> lbp_gamma_ylsb;
	vector<string> lbp_r;
	vector<string> lbp_b1;
	vector<string> lbp_b2;

	IncrDataRaw() {}

	IncrDataRaw(int valueBits) {
		lbc_cx.resize(valueBits);
		lbc_ylsb.resize(valueBits);
		lbp_gamma_cx.resize(valueBits);
		lbp_gamma_ylsb.resize(valueBits);
		lbp_r.resize(valueBits);
		lbp_b1.resize(valueBits);
		lbp_b2.resize(valueBits);
	}
};

typedef struct calcLoopArgs {
	Big a;
	Big b;
	Big p;
	Big q;
	ECn g;
	ECn h;
	ECn f;
	int bits;
	int packSize;
	time_t proofTime;
	int valueBits;
	unsigned *entrycount;
	Ledger *partialLedger;
	istream *ledger;
	ostream *proof;
	ofstream *entries;
	ofstream *incr_dst;
	unordered_map<string, IncrEntry> *incrData;
} calcLoopArgs;

typedef struct incrLoopArgs {
	Big a;
	Big b;
	Big p;
	int packSize;
	int valueBits;
	istream *incr_src;
	unordered_map<string, IncrEntry> *incrData;
} incrLoopArgs;


// The calcLoop function forms the body of a pthread, and is responsible for the bulk of the work. It performs data ingest,
// proof calculation, and data output. It does not, however, perform incremental data ingest or work with difference bits at
// all. The general methodology is this: the thread locks the ledger source and reads a group of ledger entries from it. It
// then unlocks the ledger and uses the ledger entries it has collected to generate a set of commitments and proofs. Once the
// calculations are complete, it locks the output streams one at a time and writes each output to the appropriate place.
// This process is repeated until the ledger is exhausted, at which point the thread exits. As the number of active threads
// increases, the number of ledger entries processed in each group shoud be adjusted to avoid excessive competition for
// locks, and the accompanying degredation of performance.
//
// In this implementation, all threads share a single istream for the ledger source and single ostreams for each of the data
// destinations, and each thread performs IO as it goes, keeping nothing for later except what is absolutely necessary; thus,
// each thread must wait in turn for IO. This layout is intended to conserve memory - the result is that memory consumption
// is dependant only on thread count and ledger group size, not on ledger length). In cases where memory is not a concern,
// there is no reason why the algorithm could not cache its input and/or output and perform it all at once.
void * calcLoop(void* rawArgs) {
	calcLoopArgs &args = *(static_cast<calcLoopArgs*>(rawArgs));

	// per-thread MIRACL setup
	#ifndef MR_NOFULLWIDTH
	Miracl precision(64,0);
	#else
	Miracl precision(64,MAXBASE);
	#endif

	irand(fetchRandomSeed());

	ecurve(args.a,args.b,args.p,MR_PROJECTIVE);

	bool output_entries = args.entries->is_open();
	bool output_incr = args.incr_dst->is_open();

	string identifiers[args.packSize];
	string balances[args.packSize];
	stringstream proofOutput, entriesOutput, incrOutput;
	Big cx, balance;
	int ylsb, ii, jj, kk, entrycount;
	LedgerEntry e[ENTRIES_PER_PACK_DEFAULT];

	// zl setup
	LEPProcessor lepgen(args.q, args.g, args.h, args.f, args.bits, args.incrData);
	LBPProcessor lbpgen(args.q, args.g, args.h, args.f, args.bits, args.valueBits, args.incrData);

	while (true) {

		// Begin by ingesting entries from the ledger source. If the ledger source is totally exhausted, terminate early.

		get_mip()->IOBASE=10;

		pthread_mutex_lock(&ledger_lock);

		for (ii = 0; ii < args.packSize; ii++) {
			if (!(*args.ledger >> identifiers[ii] && *args.ledger >> balances[ii])) break;
		}

		pthread_mutex_unlock(&ledger_lock);

		if (ii == 0) break;

		// Now calculate the commitments and proofs for each of the ledger entries and its bits, and cache the output locally

		get_mip()->IOBASE=DATA_BASE;

		proofOutput.str(std::string());
		entriesOutput.str(std::string());
		incrOutput.str(std::string());

		for (jj = 0; jj < ii; jj ++) {

			get_mip()->IOBASE=10;
			cinstr(balance.getbig(), (char *) balances[jj].c_str());
			get_mip()->IOBASE=DATA_BASE;
		
			e[jj] = LedgerEntry(identifiers[jj], balance, args.valueBits);

			lbpgen.genCommitments(e[jj]);
			lbpgen.genProofs(e[jj]);

			e[jj].computeR();

			lepgen.genCommitment(e[jj]);
			lepgen.genProof(e[jj]);

			// We do not need to lock before adding each entry to the ledger, because there is one partial ledger per thread.
			args.partialLedger->addEntry(e[jj]);

			ylsb = e[jj].lec.get(cx);
			proofOutput << cx << endl << ylsb << endl;
			ylsb = e[jj].lep.gamma.get(cx);
			proofOutput << cx << endl << ylsb << endl;
			proofOutput << e[jj].lep.z1 << endl;
			proofOutput << e[jj].lep.z2 << endl;
			proofOutput << e[jj].lep.z3 << endl;

			for (kk = 0; kk < args.valueBits; kk++) {
				ylsb = e[jj].lbc[kk].get(cx);
				proofOutput << cx << endl << ylsb << endl;
				ylsb = e[jj].lbp[kk].gamma1.get(cx);
				proofOutput << cx << endl << ylsb << endl;
				ylsb = e[jj].lbp[kk].gamma2.get(cx);
				proofOutput << cx << endl << ylsb << endl;
				proofOutput << e[jj].lbp[kk].c1 << endl;
				proofOutput << e[jj].lbp[kk].z1 << endl;
				proofOutput << e[jj].lbp[kk].z2 << endl;
				proofOutput << e[jj].lbp[kk].z3 << endl;
				proofOutput << e[jj].lbp[kk].z4 << endl;
			}

		}

		// Write the cached proof output and unlock if possible.

		pthread_mutex_lock(&proof_lock);
		*args.proof << proofOutput.rdbuf();
		entrycount = *args.entrycount;
		*args.entrycount += ENTRIES_PER_PACK_DEFAULT;
		pthread_mutex_unlock(&proof_lock);

		// Now export incremental and entry data if necessary and cache output locally.

		for (jj = 0; jj < ii; jj ++) {

			if (output_entries) {
				entriesOutput << entrycount << ENTRIES_EXPORT_FIELD_SEPARATOR;
				entriesOutput << e[jj].id << ENTRIES_EXPORT_FIELD_SEPARATOR;
				get_mip()->IOBASE=10;
				entriesOutput << e[jj].balance << ENTRIES_EXPORT_FIELD_SEPARATOR;
				get_mip()->IOBASE=DATA_BASE;
				entriesOutput << e[jj].r << endl;
			}

			if (output_incr) {
				incrOutput << entrycount << ENTRIES_EXPORT_FIELD_SEPARATOR;
				incrOutput << e[jj].id << ENTRIES_EXPORT_FIELD_SEPARATOR;
				get_mip()->IOBASE=10;
				incrOutput << e[jj].balance << ENTRIES_EXPORT_FIELD_SEPARATOR;
				get_mip()->IOBASE=DATA_BASE;

				for (kk = 0; kk < args.valueBits; kk++) {
					ylsb = e[jj].lbc[kk].get(cx);
					incrOutput << cx << ENTRIES_EXPORT_FIELD_SEPARATOR << ylsb << ENTRIES_EXPORT_FIELD_SEPARATOR;
				}

				ylsb = e[jj].lec.get(cx);
				incrOutput << cx << ENTRIES_EXPORT_FIELD_SEPARATOR << ylsb << ENTRIES_EXPORT_FIELD_SEPARATOR;

				for (kk = 0; kk < args.valueBits; kk++) {
					ylsb = (bit(e[jj].balance, kk) ? e[jj].lbp[kk].gamma2.get(cx) : e[jj].lbp[kk].gamma1.get(cx));
					incrOutput << cx << ENTRIES_EXPORT_FIELD_SEPARATOR << ylsb << ENTRIES_EXPORT_FIELD_SEPARATOR;
				}

				ylsb = e[jj].lep.gamma.get(cx);
				incrOutput << cx << ENTRIES_EXPORT_FIELD_SEPARATOR << ylsb << ENTRIES_EXPORT_FIELD_SEPARATOR;

				for (kk = 0; kk < args.valueBits; kk++) {
					incrOutput << e[jj].lbp[kk].r << ENTRIES_EXPORT_FIELD_SEPARATOR;
				}

				incrOutput << e[jj].r << ENTRIES_EXPORT_FIELD_SEPARATOR;

				for (kk = 0; kk < args.valueBits; kk++) {
					incrOutput << (bit(e[jj].balance, kk) ? e[jj].lbp[kk].b3 : e[jj].lbp[kk].b1) << ENTRIES_EXPORT_FIELD_SEPARATOR;
				}

				for (kk = 0; kk < args.valueBits; kk++) {
					incrOutput << (bit(e[jj].balance, kk) ? e[jj].lbp[kk].b4 : e[jj].lbp[kk].b2) << ENTRIES_EXPORT_FIELD_SEPARATOR;
				}

				incrOutput << e[jj].lep.b1 << ENTRIES_EXPORT_FIELD_SEPARATOR;
				incrOutput << e[jj].lep.b2 << ENTRIES_EXPORT_FIELD_SEPARATOR;
				incrOutput << e[jj].lep.b3;

				incrOutput << endl;
			}

			entrycount++;

		}

		// Write cached incremental and entries data.

		if (output_entries) {
			pthread_mutex_lock(&entries_lock);
			*args.entries << entriesOutput.rdbuf();
			pthread_mutex_unlock(&entries_lock);
		}

		if (output_incr) {
			pthread_mutex_lock(&incr_dst_lock);
			*args.incr_dst << incrOutput.rdbuf();
			pthread_mutex_unlock(&incr_dst_lock);
		}

		if (ii != args.packSize) break;

	}

	pthread_exit(NULL);
	return 0;
}

// The incrLoop function forms the body of a pthread, and is responsible for incremental data ingest. As with the calcLoop
// function, it attempts to keep its locks active for as little time as possible. As a consequence, it copies raw string data
// only while the incremental source lock is active, and waits to ingest it into bignums and curve points until after the
// lock is relinquished. Also as with calcLoop, ledger entries are processed in groups, and the group size can be adjusted to
// optimize performance for a particular thread count.
void * incrLoop(void* rawArgs) {
	incrLoopArgs &args = *(static_cast<incrLoopArgs*>(rawArgs));

	// per-thread MIRACL setup
	#ifndef MR_NOFULLWIDTH
	Miracl precision(64,0);
	#else
	Miracl precision(64,MAXBASE);
	#endif

	ecurve(args.a,args.b,args.p,MR_PROJECTIVE);

	Big cx;
	int ylsb;
	int ii, jj, kk;

	IncrDataRaw rawData[args.packSize];
	for (ii = 0; ii < args.packSize; ii++) {
		rawData[ii] = IncrDataRaw(args.valueBits);
	}

	IncrEntry incrData[args.packSize];
	for (ii = 0; ii < args.packSize; ii++) {
		incrData[ii] = IncrEntry(args.valueBits);
	}

	while (true) {

		// First we read the raw incremental data from the file, but do not ingest it

		pthread_mutex_lock(&incr_src_lock);

		for (ii = 0; ii < args.packSize; ii++) {
			if (!(*args.incr_src >> rawData[ii].index)) break;
			
			*args.incr_src >> rawData[ii].identifier;
			*args.incr_src >> rawData[ii].balance;
			for (int kk = 0; kk < args.valueBits; kk++) {
				*args.incr_src >> rawData[ii].lbc_cx[kk] >> rawData[ii].lbc_ylsb[kk];
			}
			*args.incr_src >> rawData[ii].lec_cx >> rawData[ii].lec_ylsb;
			for (int kk = 0; kk < args.valueBits; kk++) {
				*args.incr_src >> rawData[ii].lbp_gamma_cx[kk] >> rawData[ii].lbp_gamma_ylsb[kk];
			}
			*args.incr_src >> rawData[ii].lep_gamma_cx >> rawData[ii].lep_gamma_ylsb;
			for (int kk = 0; kk < args.valueBits; kk++) {
				*args.incr_src >> rawData[ii].lbp_r[kk];
			}
			*args.incr_src >> rawData[ii].lep_r;
			for (int kk = 0; kk < args.valueBits; kk++) {
				*args.incr_src >> rawData[ii].lbp_b1[kk];
			}
			for (int kk = 0; kk < args.valueBits; kk++) {
				*args.incr_src >> rawData[ii].lbp_b2[kk];
			}
			*args.incr_src >> rawData[ii].lep_b1;
			*args.incr_src >> rawData[ii].lep_b2;
			*args.incr_src >> rawData[ii].lep_b3;
		}

		pthread_mutex_unlock(&incr_src_lock);

		// Now that the lock is released, we actually ingest the data

		for (jj = 0; jj < ii; jj++) {
			get_mip()->IOBASE=10;
			cinstr(incrData[jj].balance.getbig(), (char *) rawData[jj].balance.c_str());
			get_mip()->IOBASE=DATA_BASE;

			for (int kk = 0; kk < args.valueBits; kk++) {
				cinstr(cx.getbig(), (char *) rawData[jj].lbc_cx[kk].c_str());
				ylsb = stoi(rawData[jj].lbc_ylsb[kk]);
				incrData[jj].lbc[kk] = ECn(cx,ylsb);
			}

			cinstr(cx.getbig(), (char *) rawData[jj].lec_cx.c_str());
			ylsb = stoi(rawData[jj].lec_ylsb);
			incrData[jj].lec = ECn(cx,ylsb);

			for (int kk = 0; kk < args.valueBits; kk++) {
				cinstr(cx.getbig(), (char *) rawData[jj].lbp_gamma_cx[kk].c_str());
				ylsb = stoi(rawData[jj].lbp_gamma_ylsb[kk]);
				incrData[jj].lbp_gamma[kk] = ECn(cx,ylsb);
			}

			cinstr(cx.getbig(), (char *) rawData[jj].lep_gamma_cx.c_str());
			ylsb = stoi(rawData[jj].lep_gamma_ylsb);
			incrData[jj].lep_gamma = ECn(cx,ylsb);

			for (int kk = 0; kk < args.valueBits; kk++) {
				cinstr(incrData[jj].lbp_r[kk].getbig(), (char *) rawData[jj].lbp_r[kk].c_str());
			}

			cinstr(incrData[jj].lep_r.getbig(), (char *) rawData[jj].lep_r.c_str());

			for (int kk = 0; kk < args.valueBits; kk++) {
				cinstr(incrData[jj].lbp_b1[kk].getbig(), (char *) rawData[jj].lbp_b1[kk].c_str());
			}

			for (int kk = 0; kk < args.valueBits; kk++) {
				cinstr(incrData[jj].lbp_b2[kk].getbig(), (char *) rawData[jj].lbp_b2[kk].c_str());
			}

			cinstr(incrData[jj].lep_b1.getbig(), (char *) rawData[jj].lep_b1.c_str());
			cinstr(incrData[jj].lep_b2.getbig(), (char *) rawData[jj].lep_b2.c_str());
			cinstr(incrData[jj].lep_b3.getbig(), (char *) rawData[jj].lep_b3.c_str());
		}

		// Finally, insert the data we've ingested into the shared data store

		pthread_mutex_lock(&incr_data_lock);
		for (jj = 0; jj < ii; jj++) {
			args.incrData->insert(pair<string, IncrEntry>(rawData[jj].identifier, incrData[jj]));
		}
		pthread_mutex_unlock(&incr_data_lock);

		if (ii != args.packSize) break;
	}

	pthread_exit(NULL);
	return 0;
}


int main(int argc, char **argv) {

	// Set up some variables to hold our options, with default values
	char* ledger_source = NULL;
	char* incr_source = NULL;
	char* proof_dest = NULL;
	char* entries_dest = NULL;
	char* incr_dest = NULL;
	char* bases_source = BASES_SOURCE_DEFAULT;
	char* curve_source = CURVE_SOURCE_DEFAULT;
	int threadcount = 0;
	int packSize = ENTRIES_PER_PACK_DEFAULT;
	int valueBits = BALANCE_BITS_DEFAULT;

	// Now read options
	int c;
	while ( (c = getopt(argc, argv, "ht:g:b:v:c:o:e:i:r:")) != -1) {
		switch (c) {
			case 'h':
				cerr << HELP_TEXT;
				return 0;
			case 't':
				threadcount = atoi(optarg);
				break;
			case 'g':
				packSize = atoi(optarg);
				break;
			case 'v':
				valueBits = atoi(optarg);
				break;
			case 'b':
				bases_source = optarg;
				break;
			case 'c':
				curve_source = optarg;
				break;
			case 'o':
				proof_dest = optarg;
				break;
			case 'e':
				entries_dest = optarg;
				break;
			case 'i':
				incr_source = optarg;
				break;
			case 'r':
				incr_dest = optarg;
				break;
			default:
				break;
		}
	}

	if (optind < argc) {
		ledger_source = argv[optind];
	}


	// MIRACL initialization
	mr_init_threading();
	#ifndef MR_NOFULLWIDTH
	Miracl precision(64,0);
	#else
	Miracl precision(64,MAXBASE);
	#endif

	irand(fetchRandomSeed());


	// Start proof output
	cerr << "ZEROLEDGE PROOF GENERATOR" << endl;
	cerr  << endl;

	fprintf(stderr, "%-40s%s", "Reading data", TAG_WORKING);
	fflush(stderr);


	// Set up curve
	ifstream curve(curve_source);
	if (curve.fail()) {
		cerr << TAG_ERASE << TAG_FAIL << endl;
		cerr << "Error: curve source could not be read." << endl;
		return 0;
	}

	get_mip()->IOBASE=16;
	int bits;
	Big a,b,p,q,x,y;
	curve >> bits >> p >> a >> b >> q >> x >> y;
	curve.close();

	ecurve(a,b,p,MR_PROJECTIVE);


	// Set up commitment bases as specified in Sections VII-A and IX-A of the paper
	ifstream seedsource(bases_source);
	if (seedsource.fail()) {
		cerr << TAG_ERASE << TAG_FAIL << endl;
		cerr << "Error: bases source could not be read.";
		return 0;
	}

	get_mip()->IOBASE=10;
	Big gseed, hseed, fseed;
	seedsource >> gseed >> hseed >> fseed;
	seedsource.close();
	
	ECn g,h,f;
	while (! g.set(gseed, 0)) {
		gseed += 1;
	}
	while (! h.set(hseed, 0)) {
		hseed += 1;
	}
	while (! f.set(fseed, 0)) {
		fseed += 1;
	}
	

	// Read incremental data if any is available
	Big cx;
	int ylsb;
	int maxThreads = (threadcount > 0) ? threadcount : sysconf( _SC_NPROCESSORS_ONLN );
	time_t proofTime = time(0);
	unordered_map<string, IncrEntry> incrData;
	get_mip()->IOBASE=DATA_BASE;

	if (incr_source != NULL) {
		ifstream incr_src(incr_source);
		if (incr_src.good()){
			incr_src >> proofTime;

			pthread_t incrThread[maxThreads];
			incrLoopArgs incrArgs[maxThreads];

			pthread_mutex_init(&incr_src_lock, NULL);
			pthread_mutex_init(&incr_data_lock, NULL);
			

			for (int ii = 0; ii < maxThreads; ii++) {
				incrArgs[ii].a = a;
				incrArgs[ii].b = b;
				incrArgs[ii].p = p;
				incrArgs[ii].packSize = packSize;
				incrArgs[ii].valueBits = valueBits;
				incrArgs[ii].incr_src = &incr_src;
				incrArgs[ii].incrData = &incrData;
				pthread_create(&(incrThread[ii]), NULL, &incrLoop, static_cast<void*>(&(incrArgs[ii])));
			}

			for (int ii = 0; ii < maxThreads; ii++) {
				pthread_join(incrThread[ii], NULL);
			}


			pthread_mutex_destroy(&incr_src_lock);
			pthread_mutex_destroy(&incr_data_lock);


			incr_src.close();
		} else {
			cerr << TAG_ERASE << TAG_FAIL << endl;
			cerr << "Error: incremental data could not be read.";
			return 0;
		}
	}


	// Prepare to read the ledger and write the various outputs
	ifstream ledgerHandle;
	streambuf * proof_buf;
	ofstream proof_output;
	ofstream entries;
	ofstream incr_dst;

	if (ledger_source != NULL) {
		ledgerHandle.open(ledger_source);
		if (ledgerHandle.fail()) {
			cerr << TAG_ERASE << TAG_FAIL << endl;
			cerr << "Error: ledger could not be read." << endl;
			return 0;
		}
	}

	if(proof_dest != NULL) {
	    proof_output.open(proof_dest);
	    if (proof_output.fail()) {
			cerr << "Error: proof destination could not be opened." << endl;
			return 0;
		}
	    proof_buf = proof_output.rdbuf();
	} else {
	    proof_buf = cout.rdbuf();
	}

	if (entries_dest != NULL) {
		entries.open(entries_dest);
		if (entries.fail()) {
			cerr << "Error: entries export destination could not be opened." << endl;
			return 0;
		}
	}

	if (incr_dest != NULL) {
		incr_dst.open(incr_dest);
		if (incr_dst.fail()) {
			cerr << "Error: incremental data export destination could not be opened." << endl;
			return 0;
		} else {
			incr_dst << proofTime << endl;
		}
	}

	cerr << TAG_ERASE << TAG_DONE << endl;
	fprintf(stderr, "%-40s%s", "Generating proof", TAG_WORKING);
	fflush(stderr);

	istream& ledger = (ledgerHandle.is_open() ? ledgerHandle : cin);
		ostream proof(proof_buf);


	// Finally, we begin reading the ledger and writing the outputs
	get_mip()->IOBASE=10;

	Big assets;
	ledger >> assets;
	
	proof << "BEGIN ZEROLEDGE PROOF" << endl;
	proof << SECTION_SEPARATOR;
	proof << endl;
	proof << "ASSETS " << assets << endl;
	proof << "TIME " << proofTime << endl;
	proof << "BITS " << valueBits << endl;
	
	proof << SECTION_SEPARATOR;
	proof << endl;

	get_mip()->IOBASE=DATA_BASE;
	
	ylsb = g.get(cx);
	proof << cx << endl << ylsb << endl;
	ylsb = h.get(cx);
	proof << cx << endl << ylsb << endl;
	ylsb = f.get(cx);
	proof << cx << endl << ylsb << endl;

	// Now fork as many threads as we are allowed to do the processing and IO.
	pthread_t thread[maxThreads];
	calcLoopArgs args[maxThreads];
	vector<Ledger> partialLedgers(maxThreads, Ledger(g, h, f, valueBits));
	unsigned entrycount = 0;

	pthread_mutex_init(&ledger_lock, NULL);
	pthread_mutex_init(&proof_lock, NULL);
	pthread_mutex_init(&entries_lock, NULL);
	pthread_mutex_init(&incr_dst_lock, NULL);

	for (int ii = 0; ii < maxThreads; ii++) {
		args[ii].a = a;
		args[ii].b = b;
		args[ii].p = p;
		args[ii].q = q;
		args[ii].g = g;
		args[ii].h = h;
		args[ii].f = f;
		args[ii].bits = bits;
		args[ii].packSize = packSize;
		args[ii].proofTime = proofTime;
		args[ii].valueBits = valueBits;
		args[ii].entrycount = &entrycount;
		args[ii].partialLedger = &partialLedgers[ii];
		args[ii].ledger = &ledger;
		args[ii].proof = &proof;
		args[ii].entries = &entries;
		args[ii].incr_dst = &incr_dst;
		args[ii].incrData = &incrData;
		pthread_create(&(thread[ii]), NULL, &calcLoop, static_cast<void*>(&(args[ii])));
	}
	
	Ledger finalLedger(g, h, f, valueBits);
	finalLedger.totalAssets = assets;

	for (int ii = 0; ii < maxThreads; ii++) {
		pthread_join(thread[ii], NULL);
		finalLedger.appendLedger(*args[ii].partialLedger);
	}

	pthread_mutex_destroy(&ledger_lock);
	pthread_mutex_destroy(&proof_lock);
	pthread_mutex_destroy(&entries_lock);
	pthread_mutex_destroy(&incr_dst_lock);

	finalLedger.computeSums();

	DBPProcessor dbpgen(q, g, h, f, bits, valueBits);
	dbpgen.genCommitments(finalLedger);
	dbpgen.genProofs(finalLedger);

	finalLedger.generateCommitments();

	proof << SECTION_SEPARATOR;
	proof << endl;

	get_mip()->IOBASE=DATA_BASE;

	for (int ii = 0; ii < valueBits; ii++) {
		ylsb = finalLedger.dbc[ii].get(cx);
		proof << cx << endl << ylsb << endl;
		ylsb = finalLedger.dbp[ii].gamma1.get(cx);
		proof << cx << endl << ylsb << endl;
		ylsb = finalLedger.dbp[ii].gamma2.get(cx);
		proof << cx << endl << ylsb << endl;
		proof << finalLedger.dbp[ii].c1 << endl;
		proof << finalLedger.dbp[ii].z1 << endl;
		proof << finalLedger.dbp[ii].z2 << endl;
		proof << finalLedger.dbp[ii].z3 << endl;
		proof << finalLedger.dbp[ii].z4 << endl;
	}

	proof << SECTION_SEPARATOR;
	proof << endl;

	proof << "END ZEROLEDGE PROOF" << endl;

	cerr << TAG_ERASE << TAG_DONE << endl;

	if (proof_dest != NULL) {
		proof_output.close();
	} else {
		cerr << endl;
	}

	if (entries.is_open()) {
		entries.close();
	}

	return 0;

}