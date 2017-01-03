#pragma once

class Transposition {
	public:
		Transposition(unsigned long long tableSize);
		void saveEntry(unsigned long long zobristKey, uint8_t depth, uint8_t age, int score, TT_FLAG flag, Move bestMove);
		int size;
		TranspositionEntry* table;
		int probe(unsigned long long zobristKey, uint8_t depth, int alpha, int beta, Move *bestMove);
	private:
};
