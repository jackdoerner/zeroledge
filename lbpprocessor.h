#ifndef LBPPROCESSOR_H
#define LBPPROCESSOR_H

#include "zeroledge.h"
#include "zlutil.h"
#include "ledger.h"

class LBPProcessor {

private:

	int bits, bytes, valuebits;
	Big q;
	ECn g, h, f;
	unordered_map<string, IncrEntry> *incrData;

public:

	// Constructor for the LBPProcessor object. Parameters are as follows
	// q:           the order of the prime field over which the elliptic curve is defined
	// g, h, f:     the bases for each of the pederson commitment components
	// workingbits: the bit length used for big integers; it must be greater than q
	// valuebits:   the number of bits to which each account balance is restricted
	LBPProcessor(Big q, ECn g, ECn h, ECn f, int workingbits, int valuebits);

	// Constructor for the LBPProcessor object. Parameters are as above, with the addition of
	// incrData:	a collection of incremental data generated along with a previous proof, indexed by account identifier.
	LBPProcessor(Big q, ECn g, ECn h, ECn f, int workingbits, int valuebits, unordered_map<string, IncrEntry> *incrData);

	// Choose a random nonce for a single ledger entry bit
	void genR(LedgerEntry &e, int ii);

	// Set the nonce for a single ledger entry bit
	void setR(LedgerEntry &e, int ii, Big r);



	// In the paper, Section VII-B (Commitment to Ledger Entry Bits) specifies the algorithm for the the generation of
	// commitments to each ledger entry bit. the LBPProcessor::genCommitment function implements that algorithm for a single
	// ledger entry bit.
	void genCommitment(LedgerEntry &e, int ii);

	// Generate a commitment to a ledger entry bit as above, using a precomputed value of gx to accellerate the process
	void genCommitment(LedgerEntry &e, int ii, ECn gx);

	// Calls the LBPProcessor::genCommitment function once for each bit, precomputing the value of gx to save time
	void genCommitments(LedgerEntry &e);



	// In the paper, Section Section VII-C (Ledger Bit Commitments) specifies the algorithm for the the generation and
	// verification of the proof that each commitment is a valid commitment and corresponds to a single bit with a value
	// of either 0 or 1. That algorithm is implemented by the following functions.

	// Begin the proof for the commitment to a single bit by performing all computations which must occur before the
	// generation of the  challenge. In an interactive zero-knowledge proof protocol, these would be the steps performed in
	// the first round by the prover. As these are bit proofs, we must not only generate the b values and gamma for the
	// actual value of the bit, but also run a simulator to generate a false (but valid) proof for the opposite value, using
	// randomly chosen values of z.
	void beginProof(LedgerEntry &e, int ii);

	// Generate the challenge for a single ledger bit proof by calling SHA-256 with the values calculated by
	// LBPProcessor::beginProof as the input. In an interactive protocol, the challenge would be chosen in the second round
	// by the verifier, but in our case the prover generates using a cryptographic hash function in accordance with the
	// Fiat-Shamir technique.
	void challengeProof(LedgerEntry &e, int ii);

	// Complete the proof by calculating the z and challenge values for the actual bit value. In an interactive protocol,
	// this step would be performed in the third round by the prover.
	void completeProof(LedgerEntry &e, int ii);

	// Generate a proof for a single ledger bit commitment, by calling each of the proof stages (LBPProcessor::beginProof,
	// LBPProcessor::challegeProof, LBPProcessor::completeProof) in sequence
	void genProof(LedgerEntry &e, int ii);

	// Calls the LBPProcessor.genProof function once for each bit
	void genProofs(LedgerEntry &e);

	// Given a ledger entry bit which has had its commitment, gamma values, challenge values, and z values assigned manually,
	// verify the proof. Returns true if proof is valid; false otherwise.
	bool verifyProof(LedgerEntry &e, int ii);

	// Calls the LBPProcessor::verifyProof function once for each bit, Returns true if all proofs are valid; false otherwise.
	bool verifyProofs(LedgerEntry &e);

};

#endif