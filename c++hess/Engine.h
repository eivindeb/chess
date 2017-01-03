#pragma once
#include "stdafx.h"
#include "Board.h"
#include "Transposition.h"
#include "Timer.h"
#include <thread>
#include <string>
#include <windows.h>

class Com;

class Engine {
	public:
		Engine(int _sideToPlay, int _depth, std::string fen = "");
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
		void comInit();
		int comReceive();
		int comUCI(std::string command);
		int comInput();
		void comSend(std::string command);
		HANDLE hstdin;
		int pipe;
		Task task;
		Mode mode;
		Timer timer;

	private:
		int maxDepth;
};