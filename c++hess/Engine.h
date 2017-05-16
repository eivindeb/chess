#pragma once
#include "stdafx.h"
#include "Board.h"
#include "Transposition.h"
#include "EvalTable.h"
#include "Evaluator.h"
#include "Timer.h"
#include <thread>
#include <string>
#include <windows.h>

class Com;

class Engine {
	public:
		Engine(int _sideToPlay, int _depth, std::string fen = "", bool console = false);
		void sortMoves(int *moves, int numOfMoves, int bestMoveId, int ply);
		int findBestMove(int *moves, int numOfMoves, int depth, int alpha, int beta, int *pvLines, int *scores);
		int miniMax(int depthLeft);
		int alphaBeta(int alpha, int beta, int depthLeft, int ply, bool allowNull, bool isPV);
		bool isRepetition();
		int contempt();
		int quiescence(int alpha, int beta);
		Board board;
		void mvvLva(int *moves, int numOfMoves);
		int sideToPlay;
		unsigned long long perft(int depth);
		int iterativeDeepening(int *moves, int numOfMoves, bool timed);
		Transposition tTable;
		void infoPV(int searchLength, int score, bool depthFinished, int pvLine=-1);
		int moveStringToInt(std::string moveString);
		void comInit();
		int comReceive();
		int comUCI(std::string command);
		int comInput();
		int comNothing(std::string command);
		void comSend(std::string command);
		int historyMoves[120][120];
		void decHistoryTable();
		int killers[50][2];
		void setKillers(int move, int ply);
		void infoNPS(unsigned long long nodes, unsigned long startTime);
		void getSearchStats(int searchDepth, int windowMissCount, unsigned long long prevNodeCount, unsigned long startTime, int *scores, int *pvLines, int pvCount);
		int SEE(int sq);
		void playGame(std::string fen = "");
		HANDLE hstdin;
		int pipe;
		Task task;
		Mode mode;
		Timer timer;
	private:
		void* boardLock;
		int orderingValues[218]; // optimization, putting it here so that it isnt initialized every time in sortMoves
		void quicksort(int *arr, int *order, const int left, const int right);
		int partition(int *arr, int *order, const int left, const int right);
		unsigned long calculateTimeForMove(Color side);
		int wmsLeft;
		int wTimeInc;
		int bmsLeft;
		int bTimeInc;
		bool stopSearch;
		int maxDepth;
		unsigned long long nodeCount;
		Color findMoveFor;
		Evaluator evaluator;
};