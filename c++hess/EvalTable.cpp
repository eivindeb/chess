#include "stdafx.h"
#include "EvalTable.h"

EvalTable::EvalTable(int tableSize) {
	size = (tableSize * 2^20 / sizeof(EvalEntry)) - 1;
	table = new EvalEntry[size];
	for (int i = 0; i < size; i++) {
		table[i] = EvalEntry{ 0, 0 };
	}
}

int EvalTable::probe(unsigned long long zobristKey) {
	EvalEntry *entry = &table[zobristKey % size];

	if (zobristKey == entry->zobristKey) {
		return entry->score;
	}

	return INVALID;
}

void EvalTable::save(unsigned long long zobristKey, int score) {
	EvalEntry *entry = &table[zobristKey % size];

	entry->zobristKey = zobristKey;
	entry->score = score;
}

