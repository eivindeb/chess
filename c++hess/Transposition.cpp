#include "stdafx.h"
#include "Transposition.h"

#define ENTRY_LIFE 0

Transposition::Transposition(unsigned long long tableSize) {
	size = (tableSize / sizeof(TranspositionEntry)) - 1;
	tableAlways = new TranspositionEntry[tableSize];
	tableDepth = new TranspositionEntry[tableSize];

	for (int i = 0; i < size; i++) {
		tableAlways[i] = TranspositionEntry{ 0, 0, 0, 0, 0, TT_INVALID };
		tableDepth[i] = TranspositionEntry{ 0, 0, 0, 0, 0, TT_INVALID };
	}
}

TT_FLAG Transposition::getPV(unsigned long long zobristKey, int *pvMove) {
	int hashIndex = zobristKey % size;
	TranspositionEntry * entry = &tableDepth[hashIndex];
	if (entry->zobristKey == zobristKey && entry->bestMove != -1) {
		*pvMove = entry->bestMove;
		return entry->flag;
	}
	else if ((entry = &tableAlways[hashIndex])->zobristKey == zobristKey && entry->bestMove != -1) {
		*pvMove = entry->bestMove;
		return entry->flag;
	}

	return TT_INVALID;
}

void Transposition::saveEntry(unsigned long long zobristKey, uint8_t depth, uint8_t age, int score, TT_FLAG flag, int bestMove) {
	int hashIndex = zobristKey % size;
	TranspositionEntry * entry = &tableDepth[hashIndex];

	//already a better (more information) entry in the table
	if (entry->zobristKey == zobristKey && entry->depth > depth) {
		return;
	}

	if (entry->age + ENTRY_LIFE < age || (entry->depth == depth && flag == TT_EXACT) || entry->depth < depth) {
		tableAlways[hashIndex] = *entry;
		entry->zobristKey = zobristKey;
		entry->depth = depth;
		entry->flag = flag;
		entry->bestMove = bestMove;
		entry->score = score;
		entry->age = age;

		return;
	}
	
	entry = &tableAlways[hashIndex];
	entry->zobristKey = zobristKey;
	entry->depth = depth;
	entry->flag = flag;
	entry->bestMove = bestMove;
	entry->score = score;
	entry->age = age;
}

int Transposition::probe(unsigned long long zobristKey, uint8_t depth, int alpha, int beta, int *bestMove) {
	int hashIndex = zobristKey % size;
	TranspositionEntry * entry = &tableDepth[hashIndex];
	if (entry->zobristKey == zobristKey || (entry = &tableAlways[hashIndex])->zobristKey == zobristKey) {
		*bestMove = entry->bestMove;

		if (entry->depth >= depth) {
			if (entry->flag == TT_EXACT) {
				return entry->score;
			}
			else if (entry->flag == TT_ALPHA && entry->score <= alpha) {
				return alpha;
			} 
			else if (entry->flag == TT_BETA && entry->score >= beta) {
				return beta;
			}
		}
	}

	return INVALID;
}

