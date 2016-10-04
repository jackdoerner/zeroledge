#ifndef LEDGER_H
#define LEDGER_H

#include <vector>
#include <unordered_map>
#include "zeroledge.h"
#include "zlutil.h"


// LedgerEntryProof represents a single ledger entry proof, excluding the initial challenge. Rather than containing its own
// methods, it is manipulated by a LEPProcessor object
struct LedgerEntryProof {
	Big b1, b2, b3, z1, z2, z3, c, b_incr, r_incr;
	ECn gamma;
};


// LedgerBitProof represents a single ledger bit proof, excluding the initial challenge, but including that bit's individual
// nonce, r. Rather than containing its own methods, it is manipulated by a LBPProcessor object. LedgerBitProof is also used
// to represent a single difference bit proof, in which case it is manipulated by a DBPProcessor object.
struct LedgerBitProof {
	Big r, b1, b2, b3, b4, z1, z2, z3, z4, c, c1, c2, b_incr, r_incr;
	ECn gamma1, gamma2;
};


// IncrEntry represents the saved incremental data for a single ledger entry. It is used to store this data during the
// incremental ingest process.
class IncrEntry {

public:

	Big balance;
	ECn lec, lep_gamma;
	Big lep_r, lep_b1, lep_b2, lep_b3;
	vector<ECn> lbc, lbp_gamma;
	vector<Big> lbp_r, lbp_b1, lbp_b2;

	IncrEntry();
	IncrEntry(int valueBits);
};


// LedgerEntry represents a single ledger entry and whatever data might be associated with it. Unlike the proof data, we use
// we use a class which includes methods for dealing with the relationships between the various data components.
class LedgerEntry {

public:

	// These variables are named differently than they are in the paper. balance is equivalent to v, idHash is equivalent to
	// x, and idHashPrime is equivalent to x'. On the other hand, this r is equivalent to r' from the paper.
	string id;
	Big idHash, balance, idHashPrime, r;
	int valueBits;

	bool incremental;
	IncrEntry incrDatum;

	ECn lec;
	LedgerEntryProof lep;
	vector<ECn> lbc;
	vector<LedgerBitProof> lbp;

	// Contstructor for LedgerEntry. Parameters are as specified before the relevant constructor variant.
	LedgerEntry();
	// valueBits:	the number of bits to which the balance is restricted
	LedgerEntry(int valueBits);
	// id:			the account ID; LedgerEntry::setId is called with this as a parameter, so that function's side effects apply.
	// balance:		the account balance.
	LedgerEntry(string id, Big balance, int valueBits);

	// Set the raw id and calculate x by taking the hash of the id, and x' as specified in Section VII-B (Commitment to
	// Ledger Entries) of the paper.
	void setId(string id);
	
	void setBalance (Big balance);
	void setR(Big r);

	// Calculate r' as specified in Section VII-B (Commitment to Ledger Entries) of the paper, based upon the values of r
	// contained in each ledger bit proof.
	void computeR();

	// Generate a new commitment using the g, h, f passed as parameters and the id, balance, and nonce stored in this object
	// and check the equivalency of that commitment with the one already stored in this object. This function is used by the
	// verifier to check the inclusion of a ledger entry in a proof, as specified in Section VII-E of the paper.
	bool verifyKnownValues(ECn g, ECn h, ECn f);

	// Verify the equivalency of the commitment to the ledger entry and the product of the commitments to its bits, as
	// specified in Section VII-D of the paper. This function is used by the verifier to check that the balance of each
	// ledger entry is positive.
	bool verifyCommitmentEquivilancy();

};


// Ledger represents a set of ledger entries, which may be a whole ledger or a part of one.
class Ledger {

public:

	ECn g, h, f;
	Big idHashSum, idHashPrimeSum, rSum;
	Big totalAssets, totalLiabilities, difference;
	ECn totalCommitment, differenceCommitment;
	int valueBits;

	vector<ECn> dbc;
	vector<Big> rBitSums;
	vector<LedgerBitProof> dbp;

	// Constructor for Ledger. Parameters are as follows:
	// g, h, f:     the bases for each of the pederson commitment components
	// valueBits:	the number of bits to which the balances and sum are restricted
	Ledger(ECn g, ECn h, ECn f, int valueBits);

	void addEntry(LedgerEntry e);
	void appendLedger(Ledger &l);

	// Compute various values which depend on all entries having been valid, but which are required before the ledger can
	// actually be used. This function must be called before any of the following functions, and before it is passed to an
	// instance of DBPProcessor
	void computeSums();

	// Generate the commitments to difference bits as specified in Section VII-B (Commitment to Difference Bits) of the
	// paper, using the total ledger bit commitments accumulated by calls to Ledger::addEntry and Ledger::appendLedger
	void generateCommitments();

	// Verify the equivalency of the commitments to the difference bits and the commitment to the assets minus the
	// liabilities, as specified Section VII-F (Proof of Solvency by Inequality). This is used by the verifier tocheck that
	// no uncomitted values have been included, and that the institution is solvent.
	bool verifyCommitmentEquivilancy();
};

#endif