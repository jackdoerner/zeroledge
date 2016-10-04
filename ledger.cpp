#include "ledger.h"
#include <cstdio>


IncrEntry::IncrEntry() {}

IncrEntry::IncrEntry(int valueBits) {
	lbc.resize(valueBits);
	lbp_gamma.resize(valueBits);
	lbp_r.resize(valueBits);
	lbp_b1.resize(valueBits);
	lbp_b2.resize(valueBits);
}



LedgerEntry::LedgerEntry() {
	this->incremental = false;
}

LedgerEntry::LedgerEntry(int valueBits) {
	this->valueBits = valueBits;
	this->lbc.resize(valueBits);
	this->lbp.resize(valueBits);

	this->incremental = false;
}

LedgerEntry::LedgerEntry(string id, Big balance, int valueBits) {
	this->valueBits = valueBits;
	this->lbc.resize(valueBits);
	this->lbp.resize(valueBits);

	this->incremental = false;
	
	this->setId(id);
	this->setBalance(balance);
}

void LedgerEntry::setId(string id) {
	this->id = id;
	this->idHash = zlhash(id.c_str(), id.length());
	this->idHashPrime = (pow(Big(2),this->valueBits) -1) * this->idHash;
}

void LedgerEntry::setBalance(Big balance) {
	this->balance = balance;
}

void LedgerEntry::setR(Big r) {
	this->r = r;
}

void LedgerEntry::computeR() {
	this->r = Big(0);
	for (int ii = 0; ii < this->valueBits; ii++) {
		this->r += pow(Big(2),ii) * this->lbp[ii].r;
	}
}

bool LedgerEntry::verifyCommitmentEquivilancy() {
	int ii;
	ECn balanceBitProduct;
	for (ii = 0; ii < this->valueBits; ii++) {
		balanceBitProduct += pow(Big(2), ii) * this->lbc[ii];
	}

	return balanceBitProduct == this->lec;
}

bool LedgerEntry::verifyKnownValues(ECn g, ECn h, ECn f) {
	ECn rhs = this->idHashPrime * g;
	rhs += this->balance * h;
	rhs += this->r * f;

	return this->lec == rhs;
}



Ledger::Ledger(ECn g, ECn h, ECn f, int valueBits) {
	this->g = g;
	this->h = h;
	this->f = f;
	this->valueBits = valueBits;
	this->dbc.resize(valueBits);
	this->dbp.resize(valueBits);
	this->totalCommitment = ECn();
	this->idHashSum = Big(0);
	this->idHashPrimeSum = Big(0);
	this->totalLiabilities = Big(0);
	this->rSum = Big(0);
	this->rBitSums.resize(valueBits);
	for (int jj = 0; jj < this->valueBits; jj++) {
		this->rBitSums[jj] = Big(0);
	}
}

void Ledger::addEntry(LedgerEntry e) {
	this->idHashSum += e.idHash;
	this->idHashPrimeSum += e.idHashPrime;
	this->totalLiabilities += e.balance;
	this->rSum += e.r;
	for (int jj = 0; jj < this->valueBits; jj++) {
		this->rBitSums[jj] += e.lbp[jj].r;
	}
	this->totalCommitment += e.lec;
}

void Ledger::appendLedger(Ledger &l) {
	this->idHashSum += l.idHashSum;
	this->idHashPrimeSum += l.idHashPrimeSum;
	this->totalLiabilities += l.totalLiabilities;
	this->rSum += l.rSum;
	for (int jj = 0; jj < this->valueBits; jj++) {
		this->rBitSums[jj] += l.rBitSums[jj];
	}
	this->totalCommitment += l.totalCommitment;
}

void Ledger::computeSums() {
	this->difference = this->totalAssets - this->totalLiabilities;
}

void Ledger::generateCommitments() {
	this->differenceCommitment = this->totalAssets * this->h;
	this->differenceCommitment -= this->totalCommitment;
}

bool Ledger::verifyCommitmentEquivilancy() {
	int ii;
	ECn differenceBitProduct;
	for (ii = 0; ii < this->valueBits; ii++) {
		differenceBitProduct += pow(Big(2), ii) * this->dbc[ii];
	}

	return differenceBitProduct == this->differenceCommitment;
}