#pragma once
#include "stdafx.h"
#include "Board.h"
#include "Transposition.h"
#include "Timer.h"
#include <string>

class Engine {
	public:
		Engine(int _sideToPlay, int _depth, std::string fen = "") : tTable(49999991), timer(20), sideToPlay(_sideToPlay), maxDepth(_depth) {
			if (fen == "") board = Board();
			else board = Board(fen);
		};
		void sortMoves(Move *moves, int numOfMoves, int bestMoveId);
		int findBestMove(Move *moves, int numOfMoves, int depth, int alpha, int beta);
		int alphaBeta(int alpha, int beta, int depthLeft);
		int quiescence(int alpha, int beta);
		int evaluatePosition();
		Board board;
		void mvvLva(Move *moves, int numOfMoves);
		int sideToPlay;
		unsigned long long perft(int depth);
		int iterativeDeepening(Move *moves, int numOfMoves);
		Transposition tTable;
		Timer timer;
	private:
		int maxDepth;
};