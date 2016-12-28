#pragma once
#include "stdafx.h"
#include "Board.h"
#include <string>

class Engine {
	public:
		Engine(int _sideToPlay, int _depth, std::string fen = "");
		int findBestMove(Move *moves, int numOfMoves);
		int alphaBeta(int alpha, int beta, int depthLeft);
		int quiescence(int alpha, int beta);
		void playGame();
		int evaluatePosition();
		Board board;
	private:
		int depth;
		int sideToPlay;
};