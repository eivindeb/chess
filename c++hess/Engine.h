#pragma once
#include "stdafx.h"
#include "Board.h"
#include "Transposition.h"
#include <string>

class Engine {
	public:
		Engine(int _sideToPlay, int _depth, std::string fen = "") : tTable(1048583), sideToPlay(_sideToPlay), maxDepth(_depth) {
			if (fen == "") board = Board();
			else board = Board(fen);
		};
		void sortMoves(Move *moves, int numOfMoves, uint8_t bestMoveId);
		int findBestMove(Move *moves, int numOfMoves, int depth, int alpha, int beta);
		int alphaBeta(int alpha, int beta, int depthLeft);
		int quiescence(int alpha, int beta);
		int evaluatePosition();
		Board board;
		void mvvLva(Move *moves, int numOfMoves);
		int sideToPlay;
		unsigned long long miniMax(int depth);
		int iterativeDeepening();
		Transposition tTable;
	private:
		int maxDepth;
};