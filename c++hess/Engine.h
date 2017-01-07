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
		Engine(int _sideToPlay, int _depth, std::string fen = "", bool console = false);
		void sortMoves(Move *moves, int numOfMoves, int bestMoveId, int ply);
		int findBestMove(Move *moves, int numOfMoves, int depth, int alpha, int beta, uint8_t *bestMoveId);
		int miniMax(int depthLeft);
		int alphaBeta(int alpha, int beta, int depthLeft, int ply, bool allowNull, bool isPV);
		bool isRepetition();
		int contempt();
		int quiescence(int alpha, int beta);
		int evaluatePosition();
		Board board;
		void mvvLva(Move *moves, int numOfMoves);
		int sideToPlay;
		unsigned long long perft(int depth);
		int iterativeDeepening(Move *moves, int numOfMoves);
		Transposition tTable;
		void infoPV(int searchLength, int score);
		void comInit();
		int comReceive();
		int comUCI(std::string command);
		int comInput();
		void comSend(std::string command);
		int historyMoves[120][120];
		void decHistoryTable();
		Move killers[50][2];
		void setKillers(Move move, int ply);
		void infoNPS(unsigned long long nodes, unsigned long startTime);
		void getSearchStats(int searchDepth, unsigned long long prevNodeCount, unsigned long startTime);
		int SEE(int sq);
		HANDLE hstdin;
		int pipe;
		Task task;
		Mode mode;
		Timer timer;
	private:
		unsigned long calculateTimeForMove(Color side);
		int wmsLeft;
		int wTimeInc;
		int bmsLeft;
		int bTimeInc;
		bool stopSearch;
		int maxDepth;
		unsigned long long nodeCount;
		Color findMoveFor;
};