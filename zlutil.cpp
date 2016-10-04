#include "zlutil.h"

unsigned int fetchRandomSeed() {
	unsigned int random_seed; 
	ifstream file(RANDOM_SOURCE, ios::binary);
	if (file.is_open()) {
		char * memblock;
		int size = sizeof(int);
		memblock = new char[size];
		file.read(memblock, size);
		file.close();
		random_seed = *reinterpret_cast<unsigned int*>(memblock);
		delete[] memblock;
	}
	return random_seed;
}

Big zlhash(const char* data, int bytes) {
	sha256 hasher;
	shs256_init(&hasher);
	int ii;
	char hash[32];
	for (ii = 0; ii < bytes; ii++) {
		shs256_process(&hasher, data[ii]);
	}
	shs256_hash(&hasher, hash);
	return from_binary(sizeof(hash), hash);
}
