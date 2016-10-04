#ifndef LEPPROCESSOR_H
#define LEPPROCESSOR_H

#include "zeroledge.h"
#include "zlutil.h"
#include "ledger.h"

class LEPProcessor {

private:

	int bits, bytes;
	Big q;
	ECn g, h, f;
	unordered_map<string, IncrEntry> *incrData;


public: 

	// Constructor for the LEPProcessor object. Parameters are as follows
	// q:           the order of the prime field over which the elliptic curve is defined
	// g, h, f:     the bases for each of the pederson commitment components
	// workingbits: the bit length used for big integers; it must be greater than q
	LEPProcessor(Big q, ECn g, ECn h, ECn f, int workingbits);

	// Constructor for the LEPProcessor object. Parameters are as above, with the addition of
	// incrData:	a collection of incremental data generated along with a previous proof, indexed by account identifier.
	LEPProcessor(Big q, ECn g, ECn h, ECn f, int workingbits, unordered_map<string, IncrEntry> *incrData);



	// In the paper, Section VII-B (Commitment to Ledger Entries) specifies the algorithm for the the generation of
	// commitments to each ledger entry. the LEPProcessor::genCommitment function implements that algorithm.
	void genCommitment(LedgerEntry &e);



	// In the paper, Section Section VII-C (Ledger Commitments) specifies the algorithm for the the generation and
	// verification of the proof that each commitment is a valid commitment and corresponds to a ledger entry with a
	// balance, identifier, and nonce known to the prover. That algorithm is implemented by the following functions.

	// Begin the proof for the commitment to a ledger entry by performing all computations which must occur before the
	// generation of the  challenge. In an interactive zero-knowledge proof protocol, these would be the steps performed in
	// the first round by the prover.
	void beginProof(LedgerEntry &e);

	// Generate the challenge for a single ledger entry proof by calling SHA-256 with the values calculated by
	// LEPProcessor::beginProof as the input. In an interactive protocol, the challenge would be chosen in the second round
	// by the verifier, but in our case the prover generates using a cryptographic hash function in accordance with the
	// Fiat-Shamir technique.
	void challengeProof(LedgerEntry &e);

	// Complete the proof by calculating the z and challenge values. In an interactive protocol, this step would be performed
	// in the third round by the prover.
	void completeProof(LedgerEntry &e);

	// Generate a proof for a single ledger entry commitment, by calling each of the proof stages (LEPProcessor::beginProof,
	// LEPProcessor::challegeProof, LEPProcessor::completeProof) in sequence
	void genProof(LedgerEntry &e);

	// Given a ledger entry bit which has had its commitment, gamma values, challenge value, and z values assigned manually,
	// verify the proof. Returns true if proof is valid; false otherwise.
	bool verifyProof(LedgerEntry &e);

};

#endif