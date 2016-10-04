#ifndef DBPPROCESSOR_H
#define DBPPROCESSOR_H

#include "zeroledge.h"
#include "zlutil.h"
#include "ledger.h"

class DBPProcessor {

private:

	int bits, bytes, valuebits;
	Big q;
	ECn g, h, f;

public: 

	// Constructor for the DBPProcessor object. Parameters are as follows
	// q:           the order of the prime field over which the elliptic curve is defined
	// g, h, f:     the bases for each of the pederson commitment components
	// workingbits: the bit length used for big integers; it must be greater than q
	// valuebits:   the number of bits to which each account balance is restricted
	DBPProcessor(Big q, ECn g, ECn h, ECn f, int workingbits, int valuebits);



	// In the paper, Section VII-B (Commitment to Difference Bits) specifies the algorithm for the the generation of
	// commitments to each difference bit. the DBPProcessor::genCommitment function implements that algorithm for a single
	// difference bit.
	void genCommitment(Ledger &l, int ii);

	// Generate a commitment to a difference bit as above, using a precomputed value of gx to accellerate the process
	void genCommitment(Ledger &l, int ii, ECn gx);

	// Calls the DBPProcessor::genCommitment function once for each bit, precomputing the value of gx to save time
	void genCommitments(Ledger &l);
	


	// In the paper, Section Section VII-C (Ledger Bit Commitments) specifies the algorithm for the the generation and
	// verification of the proof that each commitment is a valid commitment and corresponds to a single bit with a value
	// of either 0 or 1. Although this function operates on difference bits, the algorithm is identical.

	// Begin the proof for the commitment to a single bit by performing all computations which must occur before the
	// generation of the  challenge. In an interactive zero-knowledge proof protocol, these would be the steps performed in
	// the first round by the prover. As these are bit proofs, we must not only generate the b values and gamma for the
	// actual value of the bit, but also run a simulator to generate a false (but valid) proof for the opposite value, using
	// randomly chosen values of z.
	void beginProof(Ledger &l, int ii);

	// Generate the challenge for a single difference bit proof by calling SHA-256 with the values calculated by
	// DBPProcessor::beginProof as the input. In an interactive protocol, the challenge would be chosen in the second round
	// by the verifier, but in our case the prover generates using a cryptographic hash function in accordance with the
	// Fiat-Shamir technique.
	void challengeProof(Ledger &l, int ii);

	// Complete the proof by calculating the z and challenge values for the actual bit value. In an interactive protocol,
	// this step would be performed in the third round by the prover.
	void completeProof(Ledger &l, int ii);

	// Generate a proof for a single difference bit commitment, by calling each of the proof stages (DBPProcessor::beginProof,
	// DBPProcessor::challegeProof, DBPProcessor::completeProof) in sequence
	void genProof(Ledger &l, int ii);

	// Calls the LBPProcessor.genProof function once for each bit
	void genProofs(Ledger &l);

	// Given a difference bit which has had its commitment, gamma values, challenge values, and z values assigned manually,
	// verify the proof. Returns true if proof is valid; false otherwise.
	bool verifyProof(Ledger &l, int ii);

	// Calls the DBPProcessor::verifyProof function once for each bit, Returns true if all proofs are valid; false otherwise.
	bool verifyProofs(Ledger &l);

};

#endif