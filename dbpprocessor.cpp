#include "dbpprocessor.h"

DBPProcessor::DBPProcessor(Big q, ECn g, ECn h, ECn f, int workingbits, int valuebits) {
	this->q = q;
	this->g = g;
	this->h = h;
	this->f = f;
	this->bits = workingbits;
	this->bytes = bits/8;
	this->valuebits = valuebits;
}

void DBPProcessor::genCommitment(Ledger &l, int ii) {		
	l.dbc[ii] = -l.idHashSum * this->g;
	l.dbc[ii] += bit(l.difference, ii) * this->h;
	l.dbc[ii] += -l.rBitSums[ii] * this->f;
}

void DBPProcessor::genCommitment(Ledger &l, int ii, ECn gx) {		
	l.dbc[ii] = gx;
	l.dbc[ii] += -l.rBitSums[ii] * this->f;
	if (bit(l.difference, ii)) l.dbc[ii] += this->h;
	
}

void DBPProcessor::genCommitments(Ledger &l) {
	int ii;
	ECn gx = -l.idHashSum * this->g;
	for (ii = 0; ii < this->valuebits; ii++) {
		this->genCommitment(l, ii, gx);
	}
}
	
void DBPProcessor::beginProof(Ledger &l, int ii) {

	if (bit(l.difference, ii) == 0) {
		l.dbp[ii].b1 = rand(this->q);
		l.dbp[ii].b2 = rand(this->q);
		l.dbp[ii].z3 = rand(this->q);
		l.dbp[ii].z4 = rand(this->q);

		bigbits(this->bits, l.dbp[ii].c2.getbig());

		l.dbp[ii].gamma1 = l.dbp[ii].b1 * this->g;
		l.dbp[ii].gamma1 += l.dbp[ii].b2 * this->f;

		l.dbp[ii].gamma2 = l.dbp[ii].z3 * this->g;
		l.dbp[ii].gamma2 += (1 + l.dbp[ii].c2) * this->h;
		l.dbp[ii].gamma2 += l.dbp[ii].z4 * this->f;
		l.dbp[ii].gamma2 -= l.dbp[ii].c2 * l.dbc[ii];

	} else {
		l.dbp[ii].b3 = rand(this->q);
		l.dbp[ii].b4 = rand(this->q);
		l.dbp[ii].z1 = rand(this->q);
		l.dbp[ii].z2 = rand(this->q);

		bigbits(this->bits, l.dbp[ii].c1.getbig());

		l.dbp[ii].gamma1 = l.dbp[ii].z1 * this->g;
		l.dbp[ii].gamma1 += l.dbp[ii].z2 * this->f;
		l.dbp[ii].gamma1 -= l.dbp[ii].c1 * l.dbc[ii];

		l.dbp[ii].gamma2 = l.dbp[ii].b3 * this->g;
		l.dbp[ii].gamma2 += this->h;
		l.dbp[ii].gamma2 += l.dbp[ii].b4 * this->f;
	}

}

void DBPProcessor::challengeProof(Ledger &l, int ii) {
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
	l.dbc[ii].get(x, y);
	to_binary(x, this->bytes, challenge + 6 * this->bytes,  true);
	to_binary(y, this->bytes, challenge + 7 * this->bytes,  true);
	l.dbp[ii].gamma1.get(x, y);
	to_binary(x, this->bytes, challenge + 8 * this->bytes,  true);
	to_binary(y, this->bytes, challenge + 9 * this->bytes,  true);
	l.dbp[ii].gamma2.get(x, y);
	to_binary(x, this->bytes, challenge + 10 * this->bytes,  true);
	to_binary(y, this->bytes, challenge + 11 * this->bytes,  true);

	l.dbp[ii].c = zlhash(challenge, this->bytes * 12) >> (CHALLENGE_BITS - this->bits);
}

void DBPProcessor::completeProof(Ledger &l, int ii) {

	if (bit(l.difference, ii) == 0) {
		l.dbp[ii].c1 = lxor(l.dbp[ii].c, l.dbp[ii].c2);
		l.dbp[ii].z1 = (l.dbp[ii].b1 - l.dbp[ii].c1 * l.idHashSum) % this->q + this->q;
		l.dbp[ii].z2 = (l.dbp[ii].b2 - l.dbp[ii].c1 * l.rBitSums[ii]) % this->q + this->q;
	} else {
		l.dbp[ii].c2 = lxor(l.dbp[ii].c, l.dbp[ii].c1);
		l.dbp[ii].z3 = (l.dbp[ii].b3 - l.dbp[ii].c2 * l.idHashSum) % this->q + this->q;
		l.dbp[ii].z4 = (l.dbp[ii].b4 - l.dbp[ii].c2 * l.rBitSums[ii]) % this->q + this->q;
	}

}

void DBPProcessor::genProof(Ledger &l, int ii) {
	this->beginProof(l, ii);
	this->challengeProof(l, ii);
	this->completeProof(l, ii);
}

void DBPProcessor::genProofs(Ledger &l) {
	int ii;
	for (ii = 0; ii < this->valuebits; ii++) {
		this->genProof(l, ii);
	}
}

bool DBPProcessor::verifyProof(Ledger &l, int ii) {

	ECn proven1 = l.dbp[ii].z1 * this-> g;
	proven1 += l.dbp[ii].z2 * this->f;
	ECn committed1 = l.dbp[ii].c1 * l.dbc[ii];
	committed1 += l.dbp[ii].gamma1;

	ECn proven2 = l.dbp[ii].z3 * this->g;
	proven2 += (1 + l.dbp[ii].c2) * this->h;
	proven2 += l.dbp[ii].z4 * this->f;
	ECn committed2 = l.dbp[ii].c2 *  l.dbc[ii];
	committed2 += l.dbp[ii].gamma2;

	return  proven1 == committed1 && proven2 == committed2 && l.dbp[ii].c == lxor(l.dbp[ii].c1, l.dbp[ii].c2);
}

bool DBPProcessor::verifyProofs(Ledger &l) {
	bool result = true;
	int ii;
	for (ii = 0; ii < this->valuebits; ii++) {
		result &= this->verifyProof(l, ii);
	}
	return result;
}