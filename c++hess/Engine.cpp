#include "stdafx.h"
#include <windows.h>
#include "Engine.h"
#include "Communication.h"
#include <iostream>
#include <algorithm>
#include <streambuf>
#include <string>
#include <sstream>
#include <vector>

#define HASH_ORDER		1000000
#define CPT_ORDER		10000
#define PROMOTION_ORDER	10000
#define KILLER_ORDER	10000

#define DRAW_OPENING	-10
#define DRAW_ENDGAME	0
#define ENDGAME_MAT		pieceValues[KING] + pieceValues[PAWN] * 4

#define KILLER_PRIMARY		0
#define KILLER_SECONDARY	1


Engine::Engine(int _sideToPlay, int _depth, std::string fen, bool console) : tTable(49999991), timer(20), sideToPlay(_sideToPlay), maxDepth(_depth) {
	if (fen == "") board = Board();
	else board = Board(fen);
	if (!console) {
		comInit();
		std::thread t1 = std::thread([this] {this->comReceive(); });
		t1.detach();
		mode = PROTO_UCI;
	}
	else {
		mode = PROTO_NOTHING;
	}
	for (int i = 0; i < 120; i++) {
		for (int j = 0; j < 120; j++) {
			historyMoves[i][j] = 0;
		}
	}
};

int Engine::alphaBeta(int alpha, int beta, int depthLeft, int ply) {
	if (depthLeft == 0) return quiescence(alpha, beta);

	Move moves[218];
	Move ttMove = Move{ (uint8_t) 0, (uint8_t) 0, EMPTY, EMPTY, 0, (uint8_t) NO_ID };
	int score = 0;
	int bestMoveIndex = 0;
	int ttVal = 0;
	TT_FLAG ttFlag = TT_ALPHA;
	if ((ttVal = tTable.probe(board.zobristKey, depthLeft, alpha, beta, &ttMove)) != INVALID) {
		return ttVal;
	}

	int numOfMoves = 0;
	if (board.inCheck(board.sideToMove)) {
		numOfMoves = board.getLegalMovesInCheck(moves);
		if (numOfMoves == 0) {
			return -(MATE_SCORE - ply);
		}
		depthLeft++; // in check extension
	}
	else numOfMoves = board.getLegalMoves(moves);

	sortMoves(moves, numOfMoves, ttMove.id, ply);

	if (timer.timesUp) {
		return INVALID;
	}

	if (isRepetition()) return contempt();
	
	for (int i = 0; i < numOfMoves; i++) {
		board.moveMake(moves[i]);
		if (board.inCheck(Color(board.sideToMove*(-1)))) { // move that puts its own king in check
			board.moveUnmake();
			continue;
		}
		score = -alphaBeta(-beta, -alpha, depthLeft - 1, ply + 1);
		board.moveUnmake();
		if (score > alpha) {
			if (score >= beta) {
				if (!(moves[i].flags & MFLAGS_CPT)  && !(moves[i].flags & MFLAGS_PROMOTION)) {
					setKillers(moves[i], ply);
					historyMoves[moves[i].fromSq][moves[i].toSq] += depthLeft*depthLeft;
				}
				tTable.saveEntry(board.zobristKey, depthLeft, board.halfMoveCount, beta, TT_BETA, moves[i]);
				return beta;
			}
			bestMoveIndex = i;
			ttFlag = TT_EXACT;
			alpha = score;
		}
	}

	tTable.saveEntry(board.zobristKey, depthLeft, board.halfMoveCount, alpha, ttFlag, moves[bestMoveIndex]);
	return alpha;
}

int Engine::quiescence(int alpha, int beta) {
	int standPat = evaluatePosition();
	if (standPat >= beta) {
		return beta;
	}
	if (alpha < standPat) {
		alpha = standPat;
	}

	//TODO might have to check for in check here??
	Move moves[218];
	int score = 0;
	unsigned long long zobristBefore = 0;
	int numOfCaptures = board.getCaptureMoves(moves);
	mvvLva(moves, numOfCaptures);
	for (int i = 0; i < numOfCaptures; i++) {
		if (!(board.posFlag & POS_EG) && !(moves[i].flags & MFLAGS_PROMOTION) && standPat + pieceValues[moves[i].attackedPiece] + 200 < alpha) { // delta pruning
			continue;
		}
		zobristBefore = board.zobristKey;
		board.moveMake(moves[i]);
		if (board.inCheck(Color(board.sideToMove*(-1)))) {
			board.moveUnmake();
			continue;
		}
		score = -quiescence(-beta, -alpha);
		board.moveUnmake();
		if (board.zobristKey != zobristBefore) {
			zobristBefore = 0;
		}

		if (score >= beta) {
			return beta;
		}
		if (score > alpha) {
			alpha = score;
		}
	}

	return alpha;
}

// For perft checking
unsigned long long Engine::perft(int depthLeft) {
	if (depthLeft == 0) return 1;
	unsigned long long nodes = 0;
	unsigned long long nodesLast = 0;
	unsigned long long zobristBefore = 0;
	int materialBefore = 0;
	int posTotBefore = 0;

	Move moves[218];
	int numOfMoves = 0;
	if (board.inCheck(board.sideToMove)) {
		numOfMoves = board.getLegalMovesInCheck(moves);
		if (numOfMoves == 0) {
			return 0;
		}
	}
	else {
		numOfMoves = board.getLegalMoves(moves);
	}

	if (maxDepth == depthLeft) {
		std::cout << "Move\tNodes" << std::endl;
	}

	for (int i = 0; i < numOfMoves; i++) {
		materialBefore = board.materialTotal;
		posTotBefore = board.positionTotal;
		zobristBefore = board.zobristKey;
		
		board.moveMake(moves[i]);
		if (!board.inCheck(Color(board.sideToMove*(-1)))) {
			nodes += perft(depthLeft - 1);
			if (maxDepth == depthLeft) {
				std::cout << SQ_FILE(moves[i].fromSq) << SQ_RANK(moves[i].fromSq) << " " << SQ_FILE(moves[i].toSq) << SQ_RANK(moves[i].toSq) << "\t" << nodes - nodesLast << std::endl;
				nodesLast = nodes;
			}
		}
		board.moveUnmake();
		if (materialBefore != board.materialTotal || posTotBefore != board.positionTotal || zobristBefore != board.zobristKey) {
			materialBefore = 0;
		}
	}

	if (maxDepth == depthLeft) {
		std::cout << "Total nodes: " << nodes << std::endl;
	}

	return nodes;
}

int Engine::miniMax(int depthLeft) {
	if (depthLeft == 0) return evaluatePosition();
	int score;
	int scoreMax = -20000;

	Move moves[218];
	int numOfMoves = 0;
	if (board.inCheck(board.sideToMove)) {
		numOfMoves = board.getLegalMovesInCheck(moves);
		if (numOfMoves == 0) {
			return 0;
		}
	}
	else {
		numOfMoves = board.getLegalMoves(moves);
	}

	if (maxDepth == depthLeft) {
		std::cout << "Move\tScore" << std::endl;
	}

	for (int i = 0; i < numOfMoves; i++) {

		board.moveMake(moves[i]);
		if (!board.inCheck(Color(board.sideToMove*(-1)))) {
			score = -miniMax(depthLeft - 1);
			for (int j = depthLeft; j < 3; j++) {
				std::cout << "\t";
			}
			std::cout << SQ_FILE(moves[i].fromSq) << SQ_RANK(moves[i].fromSq) << " " << SQ_FILE(moves[i].toSq) << SQ_RANK(moves[i].toSq) << "\t" << score << std::endl;
		}
		if (score > scoreMax) {
			scoreMax = score;
		}
		board.moveUnmake();
	}

	return scoreMax;
}

inline void Engine::mvvLva(Move *moves, int numOfMoves) {
	std::sort(moves, moves + numOfMoves, [](Move &lhs, Move &rhs) {	return pieceSorting[lhs.attackedPiece] > pieceSorting[rhs.attackedPiece] ||
																		(pieceSorting[lhs.attackedPiece] == pieceSorting[rhs.attackedPiece] && pieceSorting[lhs.movedPiece] < pieceSorting[rhs.movedPiece]); });
}

int Engine::evaluatePosition() {
	Move moves[218];
	int wNumOfMoves;
	int bNumOfMoves;
	if (board.sideToMove == WHITE) {
		wNumOfMoves = board.getLegalMoves(moves);
		board.sideToMove = Color(board.sideToMove*(-1));
		bNumOfMoves = board.getLegalMoves(moves);
	}
	else {
		bNumOfMoves = board.getLegalMoves(moves);
		board.sideToMove = Color(board.sideToMove*(-1));
		wNumOfMoves = board.getLegalMoves(moves);
	}
	board.sideToMove = Color(board.sideToMove*(-1));
	int score = board.materialTotal + mobilityWeight * (wNumOfMoves - bNumOfMoves) + board.positionTotal;
	if (board.pieceCount[BISHOP + WHITE * 6 + 6] == 2) {
		score += pieceValues[BISHOP] * 0.1;
	}
	if (board.pieceCount[BISHOP] == 2) {
		score -= pieceValues[BISHOP] * 0.1;
	}

	return (score) * board.sideToMove;
}

//TODO, not working for black
int Engine::findBestMove(Move *moves, int numOfMoves, int depth, int alpha, int beta, uint8_t *bestMoveId) {
	int score = -20000;
	int bestId = 0;
	int bestIndex = 0;
	Move ttMove = Move{ (uint8_t)0, (uint8_t)0, EMPTY, EMPTY, 0, (uint8_t) NO_ID };
	if (board.inCheck(board.sideToMove)) { // check extension
		depth++;
	}
	tTable.probe(board.zobristKey, depth, alpha, beta, &ttMove);
	sortMoves(moves, numOfMoves, ttMove.id, 0);
	for (int i = 0; i < numOfMoves; i++) {
		if (timer.timesUp == true) {
			*bestMoveId = NO_ID;
			return 0;
		}
		board.moveMake(moves[i]);
		if (board.inCheck(Color(board.sideToMove*(-1)))) { // move that puts its own king in check
			board.moveUnmake();
			continue;
		}
		score = -alphaBeta(-beta, -alpha, depth-1, 1);
		board.moveUnmake();
		if (score > alpha) {
			alpha = score;
			*bestMoveId = moves[i].id;
			bestIndex = i;
			if (score >= beta) {
				if (!(moves[i].flags & MFLAGS_CPT) && !(moves[i].flags & MFLAGS_PROMOTION)) {
					setKillers(moves[i], 0);
					historyMoves[moves[i].fromSq][moves[i].toSq] += depth*depth;
				}
				tTable.saveEntry(board.zobristKey, depth, board.halfMoveCount, beta, TT_BETA, moves[bestIndex]);
				return beta;
			}
			tTable.saveEntry(board.zobristKey, depth, board.halfMoveCount, alpha, TT_ALPHA, moves[bestIndex]);
		}
	}

	tTable.saveEntry(board.zobristKey, depth, board.halfMoveCount, alpha, TT_EXACT, moves[bestIndex]);
	return alpha; //can return alpha to remember windows etc.
}

inline void Engine::sortMoves(Move *moves, int numOfMoves, int bestMoveId, int ply) {
	int orderingValues[218] = { 0 };
	for (int i = 0; i < numOfMoves; i++) {
		if (moves[i].flags & MFLAGS_CPT) {
			orderingValues[moves[i].id] += (pieceValues[moves[i].attackedPiece] - pieceValues[moves[i].movedPiece])*CPT_ORDER;
		}
		else {
			if (moves[i].fromSq == killers[ply][KILLER_PRIMARY].fromSq && moves[i].toSq == killers[ply][KILLER_PRIMARY].toSq) {
				orderingValues[moves[i].id] += KILLER_ORDER;
			}
			else if (moves[i].fromSq == killers[ply][KILLER_SECONDARY].fromSq && moves[i].toSq == killers[ply][KILLER_SECONDARY].toSq) {
				orderingValues[moves[i].id] += KILLER_ORDER - 1000;
			}
			else {
				orderingValues[moves[i].id] += historyMoves[moves[i].fromSq][moves[i].toSq];
			}	
		}
		if (moves[i].flags & MFLAGS_PROMOTION) {
			if (moves[i].flags & MFLAGS_PROMOTION_QUEEN){
				orderingValues[moves[i].id] += (pieceValues[QUEEN] - pieceValues[PAWN]) * PROMOTION_ORDER;
			}
			else if (moves[i].flags & MFLAGS_PROMOTION_ROOK) {
				orderingValues[moves[i].id] += (pieceValues[ROOK] - pieceValues[PAWN]) * PROMOTION_ORDER;
			}
			else if (moves[i].flags & MFLAGS_PROMOTION_BISHOP) {
				orderingValues[moves[i].id] += (pieceValues[BISHOP] - pieceValues[PAWN]) * PROMOTION_ORDER;
			}
			else orderingValues[moves[i].id] += (pieceValues[KNIGHT] - pieceValues[PAWN]) * PROMOTION_ORDER;
		}
		if (bestMoveId != 0 && bestMoveId != NO_ID && moves[i].id == bestMoveId) {
			orderingValues[moves[i].id] += HASH_ORDER;
		}
	}
	std::sort(moves, moves + numOfMoves, [&orderingValues](Move lhs, Move rhs) {return orderingValues[lhs.id] > orderingValues[rhs.id]; });
}

int Engine::iterativeDeepening(Move *moves, int numOfMoves) {
	int val;
	Color findMoveFor = board.sideToMove;
	uint8_t newBest = 0;
	uint8_t bestId = 0;
	int alpha = -200000;
	int beta = 200000;
	Move pvMove;
	timer.start(true);
	decHistoryTable();
	for (int i = 1; i <= maxDepth; i++) {
		val = findBestMove(moves, numOfMoves, i, alpha, beta, &newBest);
		if (newBest == NO_ID) {
			std::cout << "TIMES UP" << std::endl;
			break;
		}
		infoPV(i);
		bestId = newBest;
	}

	return bestId;
}

void Engine::infoPV(int searchDepth) {
	int pvLength = 0;
	TT_FLAG lastMoveFlag;
	Move pvMove;
	std::stringstream pvSS;
	switch (mode) {
		case PROTO_NOTHING:
			if (searchDepth == 1) pvSS << "PV\n";
			break;
		case PROTO_UCI:
			pvSS << "info depth " << searchDepth << " pv ";
			break;
	}
	for (int j = 0; j < searchDepth; j++) {
		if (lastMoveFlag = tTable.getPV(board.zobristKey, &pvMove)) {
			break;
		}
		pvSS << SQ_FILE(pvMove.fromSq) << SQ_RANK(pvMove.fromSq) << SQ_FILE(pvMove.toSq) << SQ_RANK(pvMove.toSq) << " ";
		board.moveMake(pvMove);
		pvLength++;
		if (lastMoveFlag != TT_EXACT) {
			break;
		}
	}
	switch (mode) {
		case PROTO_NOTHING:
			if (lastMoveFlag == TT_BETA) pvSS << ">= ";
			else if (lastMoveFlag == TT_ALPHA) pvSS << "<= ";
			pvSS << evaluatePosition()*board.sideToMove;
			break;
		case PROTO_UCI:
			if (lastMoveFlag == TT_EXACT) pvSS << "score cp ";
			else if (lastMoveFlag == TT_BETA) pvSS << "score lowerbound ";
			else pvSS << "score upperbound ";
			pvSS << evaluatePosition()*board.sideToMove;
			break;
	}

	for (int j = 0; j < pvLength; j++) {
		board.moveUnmake();
	}

	switch (mode) {
		case PROTO_NOTHING:
			std::cout << pvSS.str() << std::endl;
			break;
		case PROTO_UCI:
			comSend(pvSS.str());
	}
}


inline void Engine::decHistoryTable() {
	for (int i = 0; i < 120; i++) {
		for (int j = 0; j < 120; j++) {
			historyMoves[i][j] = historyMoves[i][j] / 8;
		}
	}
}

inline void Engine::setKillers(Move move, int ply) {
	if (move.toSq != killers[ply][KILLER_PRIMARY].toSq || move.fromSq != killers[ply][KILLER_PRIMARY].fromSq) {
		killers[ply][KILLER_SECONDARY] = killers[ply][KILLER_PRIMARY];
		killers[ply][KILLER_PRIMARY] = move;
	}
}



void Engine::comInit() {
	unsigned long dw;
	hstdin = GetStdHandle(STD_INPUT_HANDLE);
	pipe = !GetConsoleMode(hstdin, &dw);
	if (!pipe) {
		SetConsoleMode(hstdin, dw&~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
		FlushConsoleInputBuffer(hstdin);
	}
	else {
		std::cout.rdbuf()->pubsetbuf(NULL, 0);
		std::cin.rdbuf()->pubsetbuf(NULL, 0);
	}

	task = TASK_NOTHING;
	mode = PROTO_UCI;
}

int Engine::comReceive() {
	std::string command;

	if (!comInput()) return 0;


	while (1) {
		std::getline(std::cin, command);

		switch (mode) {
		//case PROTO_NOTHING: nothing(command); break;
			case PROTO_UCI:		comUCI(command);	break;
		}
	}

	return 1;
}

int Engine::comInput() {
	unsigned long dw = 0;

	if (task == TASK_NOTHING) return 1;

	//if (stdin->_cnt > 0) return 1;

	if (pipe) {
		if (!PeekNamedPipe(hstdin, 0, 0, 0, &dw, 0)) return 1;
		return dw;
	}
	else {
		GetNumberOfConsoleInputEvents(hstdin, &dw);
		if (dw > 1) task = TASK_NOTHING;
	}

	return 0;
}

int Engine::comUCI(std::string command) {
	if (command == "uci") {
		comSend("id name c ++ hess engine");
		comSend("id author Eivindeb");
		comSend("uciok");
	}
	else if (command == "isready") {
		comSend("readyok");
	}
	else if (command == "quit") {
		exit(0);
	}
	else {
		std::string buf; // Have a buffer string
		std::stringstream ss(command); // Insert the string into a stream

		std::vector<std::string> tokens; // Create vector to hold our words

		while (ss >> buf)
			tokens.push_back(buf);

		if (tokens[0] == "position") {
			if (tokens[1] == "startpos" && tokens.size() == 2) {
				board.loadFromFen(START_FEN);
			}
			else {
				std::string move = tokens.back();
				std::string moveFrom = move.substr(0, 2);
				std::string moveTo = move.substr(2);
				uint16_t flags = 0;
				if (moveTo.length() == 3) { //promotion
					flags += MFLAGS_PROMOTION;
					switch (moveTo.back()) {
					case 'q':
						flags += MFLAGS_PROMOTION_QUEEN;
						break;
					case 'r':
						flags += MFLAGS_PROMOTION_ROOK;
						break;
					case 'b':
						flags += MFLAGS_PROMOTION_BISHOP;
						break;
					case 'k':
						flags += MFLAGS_PROMOTION_KNIGHT;
						break;
					default:
						comSend("Unexpected third parameter in moveTo, was:");
						comSend(moveTo);
						break;
					}
				}
				uint8_t sqFrom = SQ_STR_TO_INT(moveFrom);
				uint8_t sqTo = SQ_STR_TO_INT(moveTo.substr(0, 2));

				if (board.board[sqTo] != EMPTY) flags += MFLAGS_CPT;
				if (sqTo == board.enPassant) flags += MFLAGS_ENP;
				if (board.board[sqFrom] == PAWN) {
					flags += MFLAGS_PAWN_MOVE;
					if (abs(sqTo - sqFrom) == 32) flags += MFLAGS_PAWN_DOUBLE;
				}

				if (board.board[sqFrom] == KING && abs(sqFrom - sqTo) == 2) {
					if (sqFrom > sqTo) {
						flags += MFLAGS_CASTLE_LONG;
					}
					else {
						flags += MFLAGS_CASTLE_SHORT;
					}
				}

				Move moveObject = { sqFrom, sqTo, board.board[sqFrom], board.board[sqTo], flags, NO_ID };
				board.moveMake(moveObject);
			}
		}
		else if (tokens[0] == "go") {
			Move moves[218];
			int numOfMoves;
			int index;
			std::stringstream moveStream;
			numOfMoves = board.getLegalMoves(moves);
			index = iterativeDeepening(moves, numOfMoves);
			for (int i = 0; i < numOfMoves; i++) {
				if (moves[i].id == index) {
					index = i;
					break;
				}
			}
			moveStream << "bestmove " << SQ_FILE(moves[index].fromSq) << SQ_RANK(moves[index].fromSq) << SQ_FILE(moves[index].toSq) << SQ_RANK(moves[index].toSq);
			comSend(moveStream.str());
			board.moveMake(moves[index]);
		}
	}

	return 0;
}

bool Engine::isRepetition() {
	for (int i = 0; i < board.repIndex; i++) {
		if (board.repStack[i] == board.zobristKey) {
			return true;
		}
	}
	return false;
}

int Engine::contempt() {
	int score = DRAW_OPENING;

	if (board.getSideMaterialValue(findMoveFor) < ENDGAME_MAT) score = DRAW_ENDGAME;

	return score * findMoveFor;
}

void Engine::comSend(std::string command) {
	std::cout << command << std::endl;
}