// zlgenerate - a ZeroLedge proof verifier implementation
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

#define HELP_TEXT "ZeroLedge Proof Verifier 1.0\n\
Usage: zlverify [\x1b[4mOPTIONS\x1b[0m] [\x1b[4mPROOF\x1b[0m]\n\
\n\
Options:\n\
  -h \t\tprint this message\n\
  -t \x1b[4mNUMBER\x1b[0m \tuse \x1b[4mNUMBER\x1b[0m threads\n\
  -b \x1b[4mPATH\x1b[0m \tread commitment base seeds from \x1b[4mPATH\x1b[0m\n\
  -c \x1b[4mPATH\x1b[0m \tread elliptic curve parameters from \x1b[4mPATH\x1b[0m\n\
  -k \x1b[4mPATH\x1b[0m \tread known ledger entries from \x1b[4mPATH\x1b[0m\n\
  -i \t\tverify ledger entry inclusion only\n"

using namespace std;

pthread_mutex_t proof_lock;
pthread_mutex_t ledger_lock;

class KnownEntry {

public:

	int index;
	string identifier;
	Big balance;
	Big r;

	KnownEntry() {}

	bool operator < (const KnownEntry& other) const
    {
        return (index < other.index);
    }

    bool operator > (const KnownEntry& other) const
    {
        return (index > other.index);
    }
};

class ProofDataRaw  {

public:

	string lec_cx, lec_ylsb;
	string lep_z1, lep_z2, lep_z3;
	string lep_gamma_cx, lep_gamma_ylsb;
	vector<string> lbc_cx;
	vector<string> lbc_ylsb;
	vector<string> lbp_gamma1_cx;
	vector<string> lbp_gamma1_ylsb;
	vector<string> lbp_gamma2_cx;
	vector<string> lbp_gamma2_ylsb;
	vector<string> lbp_c1;
	vector<string> lbp_z1;
	vector<string> lbp_z2;
	vector<string> lbp_z3;
	vector<string> lbp_z4;

	ProofDataRaw() {}

	ProofDataRaw(int valueBits) {
		lbc_cx.resize(valueBits);
		lbc_ylsb.resize(valueBits);
		lbp_gamma1_cx.resize(valueBits);
		lbp_gamma1_ylsb.resize(valueBits);
		lbp_gamma2_cx.resize(valueBits);
		lbp_gamma2_ylsb.resize(valueBits);
		lbp_c1.resize(valueBits);
		lbp_z1.resize(valueBits);
		lbp_z2.resize(valueBits);
		lbp_z3.resize(valueBits);
		lbp_z4.resize(valueBits);
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
	int valueBits;
	bool includeOnly;
	time_t proofTime;
	int *entryCount;
	istream *proof;
	Ledger *l;
	unordered_map<int, KnownEntry> *knownEntries;
	int *knownCount;
	int *correctCount;
	int *validCount;
	int *lbpValidCount;
	int *equivalencyCount;
} calcLoopArgs;


// The calcLoop function forms the body of a pthread, and is responsible for the bulk of the work. It performs data ingest
// and verification of individual ledger entry and ledger bit proofs. It does not, however, perform known entry data ingest
// As with the calcLoop function in zlgenerate.cpp, it attempts to keep its locks active for as little time as possible. As
// a consequence, it copies raw string data only while the proof source lock is active, and waits to ingest it into bignums
// and curve points until after the lock is relinquished. Also as with the calcLoop in zlgenerate.cpp, ledger entries are
// processed in groups, and the group size can be adjusted to optimize performance for a particular thread count.
void * calcLoop(void* rawArgs) {
	calcLoopArgs &args = *(static_cast<calcLoopArgs*>(rawArgs));

	// per-thread MIRACL setup
	#ifndef MR_NOFULLWIDTH
	Miracl precision(64,0);
	#else
	Miracl precision(64,MAXBASE);
	#endif

	ecurve(args.a,args.b,args.p,MR_PROJECTIVE);

	Big cx;
	int ylsb, ii, jj, kk, entryCount;
	LedgerEntry e[args.packSize];
	ProofDataRaw rawData[args.packSize];

	for (ii = 0; ii < args.packSize; ii++) {
		rawData[ii] = ProofDataRaw(args.valueBits);
	}

	// zl setup
	LEPProcessor lepgen(args.q, args.g, args.h, args.f, args.bits);
	LBPProcessor lbpgen(args.q, args.g, args.h, args.f, args.bits, args.valueBits);

	while (true) {

		// First we read the raw proof data from the file, but do not ingest it. If the proof is exhausted, or we have seen
		// all the known entries and are not interested in any others, terminate early.

		if (args.includeOnly && *args.knownCount >= args.knownEntries->size()) break;

		int knownCount = 0;

		pthread_mutex_lock(&proof_lock);

		for (ii = 0; ii < args.packSize; ii++) {
			if (args.proof->peek() == '\n') args.proof->ignore(1, '\n');
			if (args.proof->peek() == '=') break;

			*args.proof >> rawData[ii].lec_cx >> rawData[ii].lec_ylsb;
			*args.proof >> rawData[ii].lep_gamma_cx >> rawData[ii].lep_gamma_ylsb;
			*args.proof >> rawData[ii].lep_z1 >> rawData[ii].lep_z2 >> rawData[ii].lep_z3;

			for (jj = 0; jj < args.valueBits; jj++){
				*args.proof >> rawData[ii].lbc_cx[jj] >> rawData[ii].lbc_ylsb[jj];
				*args.proof >> rawData[ii].lbp_gamma1_cx[jj] >> rawData[ii].lbp_gamma1_ylsb[jj];
				*args.proof >> rawData[ii].lbp_gamma2_cx[jj] >> rawData[ii].lbp_gamma2_ylsb[jj];
				*args.proof >> rawData[ii].lbp_c1[jj];
				*args.proof >> rawData[ii].lbp_z1[jj];
				*args.proof >> rawData[ii].lbp_z2[jj];
				*args.proof >> rawData[ii].lbp_z3[jj];
				*args.proof >> rawData[ii].lbp_z4[jj];
			}
		}

		entryCount = *args.entryCount;
		*args.entryCount += ii;

		pthread_mutex_unlock(&proof_lock);

		if (ii == 0) break;

		// Now that the lock is released, we actually ingest the data and verify the ledger entry and ledger bit proofs, as
		// well as the inclusion of any entries for which we know the openers.

		get_mip()->IOBASE=DATA_BASE;

		for (jj = 0; jj < ii; jj ++) {

			if (!args.includeOnly || args.knownEntries->count(entryCount) > 0) {

				e[jj] = LedgerEntry(args.valueBits);
				cinstr(cx.getbig(), (char *) rawData[jj].lec_cx.c_str());
				ylsb = stoi(rawData[jj].lec_ylsb);
				e[jj].lec = ECn(cx, ylsb);
				cinstr(cx.getbig(), (char *) rawData[jj].lep_gamma_cx.c_str());
				ylsb = stoi(rawData[jj].lep_gamma_ylsb);
				e[jj].lep.gamma = ECn(cx,ylsb);
				lepgen.challengeProof(e[jj]);

				cinstr(e[jj].lep.z1.getbig(), (char *) rawData[jj].lep_z1.c_str());
				cinstr(e[jj].lep.z2.getbig(), (char *) rawData[jj].lep_z2.c_str());
				cinstr(e[jj].lep.z3.getbig(), (char *) rawData[jj].lep_z3.c_str());


				for (kk = 0; kk < args.valueBits; kk++){
					cinstr(cx.getbig(), (char *) rawData[jj].lbc_cx[kk].c_str());
					ylsb = stoi(rawData[jj].lbc_ylsb[kk]);
					e[jj].lbc[kk] = ECn(cx, ylsb);
					cinstr(cx.getbig(), (char *) rawData[jj].lbp_gamma1_cx[kk].c_str());
					ylsb = stoi(rawData[jj].lbp_gamma1_ylsb[kk]);
					e[jj].lbp[kk].gamma1 = ECn(cx,ylsb);
					cinstr(cx.getbig(), (char *) rawData[jj].lbp_gamma2_cx[kk].c_str());
					ylsb = stoi(rawData[jj].lbp_gamma2_ylsb[kk]);
					e[jj].lbp[kk].gamma2 = ECn(cx,ylsb);
					lbpgen.challengeProof(e[jj], kk);
					cinstr(e[jj].lbp[kk].c1.getbig(), (char *) rawData[jj].lbp_c1[kk].c_str());
					e[jj].lbp[kk].c2 = lxor(e[jj].lbp[kk].c, e[jj].lbp[kk].c1);
					cinstr(e[jj].lbp[kk].z1.getbig(), (char *) rawData[jj].lbp_z1[kk].c_str());
					cinstr(e[jj].lbp[kk].z2.getbig(), (char *) rawData[jj].lbp_z2[kk].c_str());
					cinstr(e[jj].lbp[kk].z3.getbig(), (char *) rawData[jj].lbp_z3[kk].c_str());
					cinstr(e[jj].lbp[kk].z4.getbig(), (char *) rawData[jj].lbp_z4[kk].c_str());
				}

				if (lepgen.verifyProof(e[jj])) (*args.validCount)++;
				if (lbpgen.verifyProofs(e[jj])) (*args.lbpValidCount)++;
				if (e[jj].verifyCommitmentEquivilancy()) (*args.equivalencyCount)++;
			}

			if (args.knownEntries->count(entryCount) > 0) {
				knownCount++;
				e[jj].setId(args.knownEntries->at(entryCount).identifier);
				e[jj].setBalance(args.knownEntries->at(entryCount).balance);
				e[jj].setR(args.knownEntries->at(entryCount).r);
				if (e[jj].verifyKnownValues(args.g, args.h, args.f)) (*args.correctCount)++;
			}

			entryCount++;

		}

		if (!args.includeOnly) {

			for (jj = 0; jj < ii; jj ++) {
				args.l->addEntry(e[jj]);
			}

		} else {

			pthread_mutex_lock(&ledger_lock);

			*args.knownCount += knownCount;

			pthread_mutex_unlock(&ledger_lock);

		}

		if (ii != args.packSize) break;

	}

	pthread_exit(NULL);
	return 0;
}


int main(int argc, char **argv) {

	// Set up some variables to hold our options, with default values
	bool includeOnly = false;
	char* proof_source = NULL;
	char* entries_source = NULL;
	char* bases_source = BASES_SOURCE_DEFAULT;
	char* curve_source = CURVE_SOURCE_DEFAULT;

	int threadcount = 0;

	// Now read options
	int c;
	while ( (c = getopt(argc, argv, "ht:b:c:k:i")) != -1) {
		switch (c) {
			case 'h':
				cerr << HELP_TEXT;
				return 0;
			case 't':
				threadcount = atoi(optarg);
				break;
			case 'b':
				bases_source = optarg;
				break;
			case 'c':
				curve_source = optarg;
				break;
			case 'k':
				entries_source = optarg;
				break;
			case 'i':
				includeOnly = true;
				break;
			case '?':
				return 0;
			default:
				break;
		}
	}

	if (optind < argc) {
		proof_source = argv[optind];
	}


	// MIRACL initialization
	mr_init_threading();
	#ifndef MR_NOFULLWIDTH
	Miracl precision(64,0);
	#else
	Miracl precision(64,MAXBASE);
	#endif

	// Set up curve
	ifstream curve(curve_source);
	if (curve.fail()) {
		cerr << "Error: curve source could not be read." << endl;
		return 0;
	}

	get_mip()->IOBASE=16;
	int bits;
	Big a,b,p,q,x,y;
	curve >> bits >> p >> a >> b >> q >> x >> y;
	curve.close();

	ecurve(a,b,p,MR_PROJECTIVE);

	get_mip()->IOBASE=DATA_BASE;


	// Read in the known entries, if any are available
	int ii, jj, index;
	Big balance, r;
	string identifier;
	unordered_map<int, KnownEntry> knownEntries;

	if (entries_source != NULL) {
		ifstream known(entries_source);
		if (known.good()){
			while (known >> index && known >> identifier && (get_mip()->IOBASE=10) && known >> balance && (get_mip()->IOBASE=DATA_BASE) && known >> r) {
				KnownEntry k;
				k.index = index;
				k.identifier = identifier;
				k.balance = balance;
				k.r = r;
				knownEntries.insert(pair<int, KnownEntry>(index, k));
			}
			known.close();
		}
	}


	// Now begin reading the proof
	ifstream proofHandle;
	if (proof_source != NULL) {
		proofHandle.open(proof_source);
		if (proofHandle.fail()) {
			cerr << "Error: proof could not be read." << endl;
			return 0;
		}
	}

	istream& proof = (proofHandle.is_open() ? proofHandle : cin);

	string discard;
	unsigned valueBits;
	Big assets;
	time_t proofTime;

	proof >> discard >> discard >> discard;	// BEGIN ZEROLEDGE PROOF
	proof >> discard;	// ====================

	proof >> discard;	// ASSETS
	get_mip()->IOBASE=10;
	proof >> assets;
	get_mip()->IOBASE=DATA_BASE;
	proof >> discard;	// TIME
	proof >> proofTime;
	proof >> discard;	// BITS
	proof >> valueBits;

	Big cx;
	int ylsb;
	ECn g,h,f;

	proof >> discard;	// ====================
	if (proof.peek() == '\n') proof.ignore (1, '\n');

	proof >> cx >> ylsb;
	if (proof.peek() == '\n') proof.ignore (1, '\n');
	g = ECn(cx, ylsb);
	proof >> cx >> ylsb;
	if (proof.peek() == '\n') proof.ignore (1, '\n');
	h = ECn(cx, ylsb);
	proof >> cx >> ylsb;
	if (proof.peek() == '\n') proof.ignore (1, '\n');
	f = ECn(cx, ylsb);

	DBPProcessor dbpgen(q, g, h, f, bits, valueBits);

	Ledger l(g, h, f, valueBits);
	l.totalAssets = assets;

	int entryCount = 0, correctCount = 0, validCount = 0, lbpValidCount = 0, equivalencyCount = 0,
		knownCount = 0;

	// Fork the maximum allowed threads to perform the proof ingest and verification of the individual entries.

	int maxThreads = (threadcount > 0) ? threadcount : sysconf( _SC_NPROCESSORS_ONLN );

	pthread_t thread[maxThreads];
	calcLoopArgs args[maxThreads];
	vector<Ledger> partialLedgers(maxThreads, Ledger(g, h, f, valueBits));
	vector<int> correctCounts(maxThreads, 0);
	vector<int> validCounts(maxThreads, 0);
	vector<int> lbpValidCounts(maxThreads, 0);
	vector<int> equivalencyCounts(maxThreads, 0);

	pthread_mutex_init(&ledger_lock, NULL);
	pthread_mutex_init(&proof_lock, NULL);

	for (int ii = 0; ii < maxThreads; ii++) {
		args[ii].a = a;
		args[ii].b = b;
		args[ii].p = p;
		args[ii].q = q;
		args[ii].g = g;
		args[ii].h = h;
		args[ii].f = f;
		args[ii].bits = bits;
		args[ii].packSize = ENTRIES_PER_PACK_DEFAULT;
		args[ii].valueBits = valueBits;
		args[ii].includeOnly = includeOnly;
		args[ii].proofTime = proofTime;
		args[ii].entryCount = &entryCount;
		args[ii].l = &partialLedgers[ii];
		args[ii].knownEntries = &knownEntries;
		args[ii].knownCount = &knownCount;
		args[ii].proof = &proof;
		args[ii].correctCount = &correctCounts[ii];
		args[ii].validCount = &validCounts[ii];
		args[ii].lbpValidCount = &lbpValidCounts[ii];
		args[ii].equivalencyCount = &equivalencyCounts[ii];
		pthread_create(&(thread[ii]), NULL, &calcLoop, static_cast<void*>(&(args[ii])));
	}
	
	Ledger finalLedger(g, h, f, valueBits);
	finalLedger.totalAssets = assets;

	// Collect the results from our threads, which have completed their job.

	for (int ii = 0; ii < maxThreads; ii++) {
		pthread_join(thread[ii], NULL);
		l.appendLedger(*args[ii].l);
		correctCount += *args[ii].correctCount;
		validCount += *args[ii].validCount;
		lbpValidCount += *args[ii].lbpValidCount;
		equivalencyCount += *args[ii].equivalencyCount;
	}

	pthread_mutex_destroy(&ledger_lock);
	pthread_mutex_destroy(&proof_lock);

	// Now perform our final verification procedures for per-proof elements such as bases and difference bit proofs.

	ECn gv,hv,fv;
	Big gseed, hseed, fseed;

	if (!includeOnly) {

		l.generateCommitments();

		proof >> discard;	// ====================
		if (proof.peek() == '\n') proof.ignore (1, '\n');

		for (ii = 0; ii < valueBits; ii ++) {
			proof >> cx >> ylsb;
			if (proof.peek() == '\n') proof.ignore (1, '\n');
			l.dbc[ii] = ECn(cx, ylsb);
			proof >> cx >> ylsb;
			if (proof.peek() == '\n') proof.ignore (1, '\n');
			l.dbp[ii].gamma1 = ECn(cx,ylsb);
			proof >> cx >> ylsb;
			if (proof.peek() == '\n') proof.ignore (1, '\n');
			l.dbp[ii].gamma2 = ECn(cx,ylsb);
			dbpgen.challengeProof(l, ii);
			proof >> l.dbp[ii].c1;
			l.dbp[ii].c2 = lxor(l.dbp[ii].c, l.dbp[ii].c1);
			proof >> l.dbp[ii].z1;
			proof >> l.dbp[ii].z2;
			proof >> l.dbp[ii].z3;
			proof >> l.dbp[ii].z4;
		}

		// Verify up commitment bases as specified in Sections VII-A and IX-A of the paper
		ifstream seedsource(bases_source);
		if (seedsource.fail()) {
			cerr << "Error: bases source could not be read." << endl;;
			return 0;
		}

		get_mip()->IOBASE=10;
		seedsource >> gseed >> hseed >> fseed;
		seedsource.close();
		
		while (! gv.set(gseed, 0)) {
			gseed += 1;
		}
		while (! hv.set(hseed, 0)) {
			hseed += 1;
		}
		while (! fv.set(fseed, 0)) {
			fseed += 1;
		}

	}

	cout << "ZEROLEDGE PROOF VERIFIER" << endl;

	cout << endl;

	cout << "Ledger Entries: " << entryCount << endl;
	cout << "Maximum Liability: " << assets << endl;
	cout << "Proof Time: " << ctime(&proofTime);
	cout << "Validating..." << endl;
	
	cout << endl;

	bool basesValidated;

	if (!includeOnly) {

		// Check Bases
		printf("%-40s%s", "Bases", TAG_WORKING);
		fflush(stdout);
		basesValidated = gv == g && hv == h && fv == f;
		cout << TAG_ERASE << (basesValidated ? TAG_VALID : TAG_INVALID) << endl;

	}

	if (knownEntries.size() > 0) printf("%-40s%s\n", "Known Ledger Entries", (correctCount == knownEntries.size() ? TAG_VALID : TAG_INVALID));

	if (includeOnly) return 0;

	// Check Ledger Entry Proofs
	printf("%-40s%s\n", "Ledger Entry Proofs", (validCount == entryCount ? TAG_VALID : TAG_INVALID));

	// Check Ledger Bit Proofs
	printf("%-40s%s\n", "Ledger Bit Proofs", (lbpValidCount == entryCount ? TAG_VALID : TAG_INVALID));

	// Check Ledger Entry Equivalency
	printf("%-40s%s\n", "Ledger Commitment Equivalency", (equivalencyCount == entryCount ? TAG_VALID : TAG_INVALID));

	// Check Ledger Entry Proofs
	printf("%-40s%s", "Difference Bit Proofs", TAG_WORKING);
	fflush(stdout);
	bool differenceBitsValidated = dbpgen.verifyProofs(l);
	cout << TAG_ERASE << (differenceBitsValidated ? TAG_VALID : TAG_INVALID) << endl;

	// Check Overall Equivalency
	printf("%-40s%s", "Total Commitment Equivalency", TAG_WORKING);
	fflush(stdout);
	bool equivalencyValidated = l.verifyCommitmentEquivilancy();
	cout << TAG_ERASE << (equivalencyValidated ? TAG_VALID : TAG_INVALID) << endl;

	// Final Report
	bool proofOK = (validCount == entryCount)
				&& (correctCount == knownEntries.size())
				&& (lbpValidCount == entryCount)
				&& (equivalencyCount == entryCount)
				&& basesValidated
				&& differenceBitsValidated
				&& equivalencyValidated;

	printf("%-40s%s\n", "ZeroLedge Proof", (proofOK ? TAG_VALID : TAG_INVALID));

	return 0;
}