#include "lbpprocessor.h"

LBPProcessor::LBPProcessor(Big q, ECn g, ECn h, ECn f, int workingbits, int valuebits) {
	this->q = q;
	this->g = g;
	this->h = h;
	this->f = f;
	this->bits = workingbits;
	this->bytes = bits/8;
	this->valuebits = valuebits;
	this->incrData = NULL;
}

LBPProcessor::LBPProcessor(Big q, ECn g, ECn h, ECn f, int workingbits, int valuebits, unordered_map<string, IncrEntry> *incrData) {
	this->q = q;
	this->g = g;
	this->h = h;
	this->f = f;
	this->bits = workingbits;
	this->bytes = bits/8;
	this->valuebits = valuebits;
	this->incrData = incrData;
}

void LBPProcessor::genR(LedgerEntry &e, int ii) {
	e.lbp[ii].r = rand(this->q);
}

void LBPProcessor::setR(LedgerEntry &e, int ii, Big r) {
	e.lbp[ii].r = Big(r);
}

void LBPProcessor::genCommitment(LedgerEntry &e, int ii) {		
	e.lbc[ii] = e.idHash * this->g;
	e.lbc[ii] += e.lbp[ii].r * this->f;
	if (bit(e.balance, ii)) e.lbc[ii] += this->h;
}

void LBPProcessor::genCommitment(LedgerEntry &e, int ii, ECn gx) {		
	e.lbc[ii] = gx;
	e.lbc[ii] += e.lbp[ii].r * this->f;
	if (bit(e.balance, ii)) e.lbc[ii] += this->h;
}

void LBPProcessor::genCommitments(LedgerEntry &e) {
	int ii;
	ECn gx = e.idHash * this->g;
	for (ii = 0; ii < this->valuebits; ii++) {
		this->genR(e, ii);
		this->genCommitment(e, ii, gx);
	}
}
	
void LBPProcessor::beginProof(LedgerEntry &e, int ii) {


	if (this->incrData && this->incrData->count(e.id)) {
		e.incremental = true;
		e.incrDatum = this->incrData->at(e.id);
		e.lbp[ii].b_incr = rand(this->q);
	}

	if (bit(e.balance, ii) == 0) {
		
		if (e.incremental && bit(e.incrDatum.balance, ii) == 0) {
			e.lbp[ii].b1 = e.incrDatum.lbp_b1[ii] * e.lbp[ii].b_incr;
			e.lbp[ii].b2 = e.incrDatum.lbp_b2[ii] * e.lbp[ii].b_incr;
			e.lbp[ii].gamma1 = e.lbp[ii].b_incr * e.incrDatum.lbp_gamma[ii];
		} else {
			e.lbp[ii].b1 = rand(this->q);
			e.lbp[ii].b2 = rand(this->q);
			e.lbp[ii].gamma1 = e.lbp[ii].b1 * this->g;
			e.lbp[ii].gamma1 += e.lbp[ii].b2 * this->f;
		}

		e.lbp[ii].z3 = rand(this->q);
		e.lbp[ii].z4 = rand(this->q);
		bigbits(this->bits, e.lbp[ii].c2.getbig());

		e.lbp[ii].gamma2 = e.lbp[ii].z3 * this->g;
		e.lbp[ii].gamma2 += (1 + e.lbp[ii].c2) * this->h;
		e.lbp[ii].gamma2 += e.lbp[ii].z4 * this->f;
		e.lbp[ii].gamma2 -= e.lbp[ii].c2 * e.lbc[ii];

	} else {
		if (e.incremental && bit(e.incrDatum.balance, ii) == 1) {
			e.lbp[ii].b3 = e.incrDatum.lbp_b1[ii] * e.lbp[ii].b_incr;
			e.lbp[ii].b4 = e.incrDatum.lbp_b2[ii] * e.lbp[ii].b_incr;
			e.lbp[ii].gamma2 = e.incrDatum.lbp_gamma[ii];
			e.lbp[ii].gamma2 -= this->h;
			e.lbp[ii].gamma2 *= e.lbp[ii].b_incr; 
			e.lbp[ii].gamma2 += this->h;
		} else {
			e.lbp[ii].b3 = rand(this->q);
			e.lbp[ii].b4 = rand(this->q);
			e.lbp[ii].gamma2 = e.lbp[ii].b3 * this->g;
			e.lbp[ii].gamma2 += this->h;
			e.lbp[ii].gamma2 += e.lbp[ii].b4 * this->f;
		}

		e.lbp[ii].z1 = rand(this->q);
		e.lbp[ii].z2 = rand(this->q);
		bigbits(this->bits, e.lbp[ii].c1.getbig());

		e.lbp[ii].gamma1 = e.lbp[ii].z1 * this->g;
		e.lbp[ii].gamma1 += e.lbp[ii].z2 * this->f;
		e.lbp[ii].gamma1 -= e.lbp[ii].c1 * e.lbc[ii];

	}

}

void LBPProcessor::challengeProof(LedgerEntry &e, int ii) {
		char challenge[this->bytes * 12];

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
	e.lbc[ii].get(x, y);
	to_binary(x, this->bytes, challenge + 6 * this->bytes,  true);
	to_binary(y, this->bytes, challenge + 7 * this->bytes,  true);
	e.lbp[ii].gamma1.get(x, y);
	to_binary(x, this->bytes, challenge + 8 * this->bytes,  true);
	to_binary(y, this->bytes, challenge + 9 * this->bytes,  true);
	e.lbp[ii].gamma2.get(x, y);
	to_binary(x, this->bytes, challenge + 10 * this->bytes,  true);
	to_binary(y, this->bytes, challenge + 11 * this->bytes,  true);

	e.lbp[ii].c = zlhash(challenge, this->bytes * 12) >> (CHALLENGE_BITS - this->bits);
}

void LBPProcessor::completeProof(LedgerEntry &e, int ii) {

	if (bit(e.balance, ii) == 0) {
		e.lbp[ii].c1 = lxor(e.lbp[ii].c, e.lbp[ii].c2);
		e.lbp[ii].z1 = (e.lbp[ii].b1 + e.lbp[ii].c1 * e.idHash) % this->q + this->q;
		e.lbp[ii].z2 = (e.lbp[ii].b2 + e.lbp[ii].c1 * e.lbp[ii].r) % this->q + this->q;
	} else {
		e.lbp[ii].c2 = lxor(e.lbp[ii].c, e.lbp[ii].c1);
		e.lbp[ii].z3 = (e.lbp[ii].b3 + e.lbp[ii].c2 * e.idHash) % this->q + this->q;
		e.lbp[ii].z4 = (e.lbp[ii].b4 + e.lbp[ii].c2 * e.lbp[ii].r) % this->q + this->q;
	}

}

void LBPProcessor::genProof(LedgerEntry &e, int ii) {
	this->beginProof(e, ii);
	this->challengeProof(e, ii);
	this->completeProof(e, ii);
}

void LBPProcessor::genProofs(LedgerEntry &e) {
	int ii;
	for (ii = 0; ii < this->valuebits; ii++) {
		this->genProof(e, ii);
	}
}

bool LBPProcessor::verifyProof(LedgerEntry &e, int ii) {

	ECn proven1 = e.lbp[ii].z1 * this->g;
	proven1 += e.lbp[ii].z2 * this->f;
	ECn committed1 = e.lbp[ii].c1 * e.lbc[ii];
	committed1 += e.lbp[ii].gamma1;

	ECn proven2 = e.lbp[ii].z3 * this->g;
	proven2 += (1 + e.lbp[ii].c2) * this->h;
	proven2 += e.lbp[ii].z4 * this->f;
	ECn committed2 = e.lbp[ii].c2 *  e.lbc[ii];
	committed2 += e.lbp[ii].gamma2;

	return  proven1 == committed1 && proven2 == committed2 && e.lbp[ii].c == lxor(e.lbp[ii].c1, e.lbp[ii].c2);
}

bool LBPProcessor::verifyProofs(LedgerEntry &e) {
	bool result = true;
	int ii;
	for (ii = 0; ii < this->valuebits; ii++) {
		result &= this->verifyProof(e, ii);
	}
	return result;
}