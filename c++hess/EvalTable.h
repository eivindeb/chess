#pragma once

class EvalTable {
public:
	EvalTable(int tableSize);
	int probe(unsigned long long zobristKey);
	void save(unsigned long long zobristKey, int score);
	EvalEntry *table;
private:
	int size;
};