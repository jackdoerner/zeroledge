#include "lepprocessor.h"

LEPProcessor::LEPProcessor(Big q, ECn g, ECn h, ECn f, int workingbits) {
	this->q = q;
	this->g = g;
	this->h = h;
	this->f = f;
	this->bits = workingbits;
	this->bytes = bits/8;
	this->incrData = NULL;
}

LEPProcessor::LEPProcessor(Big q, ECn g, ECn h, ECn f, int workingbits, unordered_map<string, IncrEntry> *incrData) {
	this->q = q;
	this->g = g;
	this->h = h;
	this->f = f;
	this->bits = workingbits;
	this->bytes = bits/8;
	this->incrData = incrData;
}

void LEPProcessor::genCommitment(LedgerEntry &e) {
	if (e.incremental) {
		e.lec = (e.r - e.incrDatum.lep_r) * this->f;
		e.lec += e.incrDatum.lec;
		if (e.balance - e.incrDatum.balance != 0) {
			e.lec += (e.balance - e.incrDatum.balance) * this->h;
		}
	} else {		
		e.lec = e.idHashPrime * this->g;
		e.lec += e.balance * this->h;
		e.lec += e.r * this->f;
	}
}

void LEPProcessor::beginProof(LedgerEntry &e) {
	if (this->incrData && this->incrData->count(e.id)) {
		e.incremental = true;
		e.incrDatum = this->incrData->at(e.id);
		e.lep.b_incr = rand(this->q);
		e.lep.b1 = e.incrDatum.lep_b1 * e.lep.b_incr;
		e.lep.b2 = e.incrDatum.lep_b2 * e.lep.b_incr;
		e.lep.b3 = e.incrDatum.lep_b3 * e.lep.b_incr;
		e.lep.gamma = e.lep.b_incr * e.incrDatum.lep_gamma;
	} else {
		e.lep.b1 = rand(this->q);
		e.lep.b2 = rand(this->q);
		e.lep.b3 = rand(this->q);
		e.lep.gamma = e.lep.b1 * this->g;
		e.lep.gamma += e.lep.b2 * this->h;
		e.lep.gamma += e.lep.b3 * this->f;
	}	
}

void LEPProcessor::challengeProof(LedgerEntry &e) {
	char challenge[this->bytes * 10];

	Big x, y;
	this->g.get(x, y);
	to_binary(x, this->bytes, challenge + 0 * this->bytes,  true);
	to_binary(y, this->bytes, challenge + 1 * this->bytes,  true);
	this->h.get(x, y);
	to_binary(x, this->bytes, challenge + 2 * this->bytes,  true);
	to_binary(y, this->bytes, challenge + 3 * this->bytes,  true);
	this->f.get(x, y);
	to_binary(x, this->bytes, challenge + 4 * this->bytes,  true);
	to_binary(y, this->bytes, challenge + 5 * this->bytes,  true);
	e.lec.get(x, y);
	to_binary(x, this->bytes, challenge + 6 * this->bytes,  true);
	to_binary(y, this->bytes, challenge + 7 * this->bytes,  true);
	e.lep.gamma.get(x, y);
	to_binary(x, this->bytes, challenge + 8 * this->bytes,  true);
	to_binary(y, this->bytes, challenge + 9 * this->bytes,  true);

	e.lep.c = zlhash(challenge, this->bytes * 10) >> (CHALLENGE_BITS - this->bits);
}

void LEPProcessor::completeProof(LedgerEntry &e) {
	e.lep.z1 = (e.lep.b1 + e.lep.c * e.idHashPrime) % this->q;
	e.lep.z2 = (e.lep.b2 + e.lep.c * e.balance) % this->q;
	e.lep.z3 = (e.lep.b3 + e.lep.c * e.r) % this->q;
}

bool LEPProcessor::verifyProof(LedgerEntry &e) {
	ECn proven = e.lep.z1 * this->g;
	proven += e.lep.z2 * this->h;
	proven += e.lep.z3 * this->f;
	ECn committed = e.lec;
	committed *= e.lep.c;
	committed += e.lep.gamma;
	return  proven == committed;
}

void LEPProcessor::genProof(LedgerEntry &e) {
	this->beginProof(e);
	this->challengeProof(e);
	this->completeProof(e);
}