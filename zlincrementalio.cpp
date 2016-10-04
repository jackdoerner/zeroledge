// zlincrementalio - a ZeroLedge proof generator implementation benchmarking tool
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
#include <pthread.h>

#include "zeroledge.h"
#include "zlutil.h"
#include "ledger.h"
#include "lepprocessor.h"
#include "lbpprocessor.h"
#include "dbpprocessor.h"

#define HELP_TEXT "ZeroLedge Incremental IO Benchmark 1.0\n\
Usage: zlincrementalio [\x1b[4mOPTIONS\x1b[0m]\n\
\n\
Options:\n\
  -h \t\tprint this message\n\
  -t \x1b[4mNUMBER\x1b[0m \tuse \x1b[4mNUMBER\x1b[0m threads\n\
  -g \x1b[4mNUMBER\x1b[0m \tprocess \x1b[4mNUMBER\x1b[0m entries at a time\n\
  -v \x1b[4mNUMBER\x1b[0m \trestrict balances and sums to \x1b[4mNUMBER\x1b[0m bits\n\
  -b \x1b[4mPATH\x1b[0m \tread commitment base seeds from \x1b[4mPATH\x1b[0m\n\
  -c \x1b[4mPATH\x1b[0m \tread elliptic curve parameters from \x1b[4mPATH\x1b[0m\n\
  -i \x1b[4mPATH\x1b[0m \tgenerate incremental proof using data from \x1b[4mPATH\x1b[0m\n"

using namespace std;

pthread_mutex_t incr_src_lock;
pthread_mutex_t incr_data_lock;

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

typedef struct incrLoopArgs {
	Big a;
	Big b;
	Big p;
	int packSize;
	int valueBits;
	istream *incr_src;
	unordered_map<string, IncrEntry> *incrData;
} incrLoopArgs;


// The incrLoop function forms the body of a pthread, and is responsible for incremental data ingest. As with the calcLoop
// function in zlgenerate.cpp, it attempts to keep its locks active for as little time as possible. As a consequence, it 
// copies raw string data only while the incremental source lock is active, and waits to ingest it into bignums and curve 
// points until after the lock is relinquished. Also as with the calcLoop in zlgenerate.cpp, ledger entries are processed
// in groups, and the group size can be adjusted to optimize performance for a particular thread count.
void * incrLoop(void* rawArgs) {
	incrLoopArgs &args = *(static_cast<incrLoopArgs*>(rawArgs));

	// per-thread MIRACL setup
	#ifndef MR_NOFULLWIDTH
	Miracl precision(64,0);
	#else
	Miracl precision(64,MAXBASE);
	#endif

	ecurve(args.a,args.b,args.p,MR_PROJECTIVE);

	// temp vars
	IncrDataRaw rawData[args.packSize];
	for (ii = 0; ii < args.packSize; ii++) {
		rawData[ii] = IncrDataRaw(args.valueBits);
	}

	IncrEntry incrData[args.packSize];
	for (ii = 0; ii < args.packSize; ii++) {
		incrData[ii] = IncrEntry(args.valueBits);
	}

	Big cx;
	int ylsb;
	int ii, jj, kk;

	while (true) {

		// First we read the raw incremental data from the file, but do not ingest it

		pthread_mutex_lock(&incr_src_lock);

		for (ii = 0; ii < args.pack_size; ii++) {
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

		if (ii != args.pack_size) break;
	}

	pthread_exit(NULL);
	return 0;
}


int main(int argc, char **argv) {

	// Set up some variables to hold our options, with default values
	char* incr_source = NULL;
	char* bases_source = BASES_SOURCE_DEFAULT;
	char* curve_source = CURVE_SOURCE_DEFAULT;
	int threadcount = 0;
	int pack_size = ENTRIES_PER_PACK_DEFAULT;
	int valueBits = BALANCE_BITS_DEFAULT;

	// Now read options
	int c;
	while ( (c = getopt(argc, argv, "ht:g:b:c:i:")) != -1) {
		switch (c) {
			case 'h':
				cerr << HELP_TEXT;
				return 0;
			case 't':
				threadcount = atoi(optarg);
				break;
			case 'g':
				pack_size = atoi(optarg);
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
			case 'i':
				incr_source = optarg;
				break;
			default:
				break;
		}
	}

	// MIRACL setup
	mr_init_threading();
	#ifndef MR_NOFULLWIDTH
	Miracl precision(64,0);
	#else
	Miracl precision(64,MAXBASE);
	#endif

	irand(fetchRandomSeed());


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
	get_mip()->IOBASE=10;
	Big gseed, hseed, fseed;
	seedsource >> gseed >> hseed >> fseed;
	seedsource.close();

	ifstream seedsource(bases_source);
	if (seedsource.fail()) {
		cerr << TAG_ERASE << TAG_FAIL << endl;
		cerr << "Error: bases source could not be read.";
		return 0;
	}
	
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
				incrArgs[ii].pack_size = pack_size;
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

	return 0;

}