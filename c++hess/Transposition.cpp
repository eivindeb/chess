#include "stdafx.h"
#include "Transposition.h"

Transposition::Transposition(unsigned long long tableSize) {
	size = (tableSize / sizeof(TranspositionEntry)) - 1;
	table = new TranspositionEntry[tableSize];
}

TT_FLAG Transposition::getPV(unsigned long long zobristKey, int *pvMove) {
	TranspositionEntry * entry = &table[zobristKey % size];
	if (entry->zobristKey == zobristKey && entry->bestMove == -1) {
		*pvMove = entry->bestMove;
		return entry->flag;
	}
	return TT_INVALID;
}

void Transposition::saveEntry(unsigned long long zobristKey, uint8_t depth, uint8_t age, int score, TT_FLAG flag, int bestMove) {
	TranspositionEntry * entry = &table[zobristKey % size];

	//already a better (more information) entry in the table
	if (entry->zobristKey == zobristKey && entry->depth > depth) {
		return;
	}

	entry->zobristKey = zobristKey;
	entry->depth = depth;
	entry->flag = flag;
	entry->bestMove = bestMove;
	entry->score = score;
	entry->age = age;
}

int Transposition::probe(unsigned long long zobristKey, uint8_t depth, int alpha, int beta, int *bestMove) {
	TranspositionEntry * entry = &table[zobristKey % size];
	if (entry->zobristKey == zobristKey) {
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

