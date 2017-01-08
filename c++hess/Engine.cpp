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
#include <math.h>

#define HASH_ORDER		10000000
#define CPT_ORDER		5000
#define PROMOTION_ORDER	5000
#define KILLER_ORDER	1000
#define SEE_ORDER_DEPTH	0

#define DRAW_OPENING	-10
#define DRAW_ENDGAME	0
#define ENDGAME_MAT		pieceValues[KING] + pieceValues[PAWN] * 4

#define KILLER_PRIMARY		0
#define KILLER_SECONDARY	1

#define DEPTH_TIME_INCREASE 2

#define ALLOW_NULL	true
#define IS_PV		true
#define NOT_PV		false

#define WINDOW_SIZE pieceValues[PAWN] / 2

#define R	2	//reduction depth

#define FIXED_SEARCH_DURATION	30*1000;

//49999991

Engine::Engine(int _sideToPlay, int _depth, std::string fen, bool console) : tTable(4194311), timer(), sideToPlay(_sideToPlay), maxDepth(_depth) {
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
	wmsLeft = 0;
	bmsLeft = 0;
};

int Engine::alphaBeta(int alpha, int beta, int depthLeft, int ply, bool allowNull, bool isPV) {
	nodeCount++;
	bool inCheck = board.inCheck(board.sideToMove);
	if (inCheck) depthLeft++; // in check extension
	if (depthLeft == 0) return quiescence(alpha, beta);

	Move moves[218];
	Move ttMove = Move{ (uint8_t)0, (uint8_t)0, EMPTY, EMPTY, 0, (uint8_t)NO_ID };
	int score = 0;
	int bestMoveIndex = 0;
	int ttVal = 0;
	bool raisedAlpha = false;
	bool fPrune = false;

	int newDepth = depthLeft - 1;

	TT_FLAG ttFlag = TT_ALPHA;
	if ((ttVal = tTable.probe(board.zobristKey, depthLeft, alpha, beta, &ttMove)) != INVALID) {
		return ttVal;
	}

	if (depthLeft > 2 && allowNull && !(board.phaseFlag & PHASE_EG) && !isPV && !inCheck) { // null move pruning
		board.moveMakeNull();
		score = -alphaBeta(-beta, -beta + 1, newDepth - R, ply, !ALLOW_NULL, NOT_PV);
		board.moveUnmakeNull();
		if (score >= beta) return beta;
	}

	/*if (depthLeft == 1 && !inCheck && (evaluatePosition() + pieceValues[BISHOP]) < alpha) { // futility pruning
		fPrune = true;
	}
	else if (depthLeft == 2 && !inCheck && (evaluatePosition() + pieceValues[ROOK]) < alpha) {
		fPrune = true;
	}
	else if (depthLeft == 3 && !inCheck && (evaluatePosition() + pieceValues[QUEEN]) < alpha) {
		fPrune = true;
	}*/

	int numOfMoves = 0;
	if (inCheck) {
		numOfMoves = board.getLegalMovesInCheck(moves);
		if (numOfMoves == 0) {
			return -(MATE_SCORE - ply);
		}
	}
	else numOfMoves = board.getLegalMoves(moves);

	sortMoves(moves, numOfMoves, ttMove.id, ply);

	if (!(nodeCount & 4000) && (timer.timesUp || stopSearch)) {
		return 0;
	}

	if (isRepetition()) return contempt();
	
	for (int i = 0; i < numOfMoves; i++) {
		board.moveMake(moves[i]);
		if (board.inCheck(Color(board.sideToMove*(-1)))) { // move that puts its own king in check
			board.moveUnmake();
			continue;
		}
		newDepth = depthLeft - 1;

		if (i >= 4 && !isPV && depthLeft >= 3 && !(moves[i].flags & MFLAGS_CPT) && !(moves[i].flags & MFLAGS_PROMOTION) && !(board.inCheck(board.sideToMove))) {
			newDepth--;
		}

		/*if (fPrune && !(moves[i].flags & MFLAGS_CPT) && !(moves[i].flags & MFLAGS_PROMOTION) && !(board.inCheck(board.sideToMove))) { // futility pruning
			board.moveUnmake();
			continue;
		}*/

		if (!raisedAlpha) { // principal variation search
			score = -alphaBeta(-beta, -alpha, newDepth, ply + 1, ALLOW_NULL, isPV);
		}
		else {
			if (-alphaBeta(-alpha - 1, -alpha, newDepth, ply + 1, ALLOW_NULL, NOT_PV) > alpha) {
				score = -alphaBeta(-beta, -alpha, newDepth, ply + 1, ALLOW_NULL, IS_PV);
			}
		}
		
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
			raisedAlpha = true;
			alpha = score;
		}
	}

	tTable.saveEntry(board.zobristKey, depthLeft, board.halfMoveCount, alpha, ttFlag, moves[bestMoveIndex]);
	return alpha;
}

int Engine::quiescence(int alpha, int beta) {
	nodeCount++;
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
	int numOfCaptures = board.getCaptureMoves(moves);
	mvvLva(moves, numOfCaptures);
	for (int i = 0; i < numOfCaptures; i++) {
		if (!(board.phaseFlag & PHASE_EG) && !(moves[i].flags & MFLAGS_PROMOTION) && standPat + pieceValues[moves[i].attackedPiece] + 200 < alpha) { // delta pruning
			continue;
		}
		//if (SEE(moves[i].toSq) < 0) {
		//	continue;
		//}
		board.moveMake(moves[i]);
		if (board.inCheck(Color(board.sideToMove*(-1)))) {
			board.moveUnmake();
			continue;
		}
		score = -quiescence(-beta, -alpha);
		board.moveUnmake();

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

int Engine::findBestMove(Move *moves, int numOfMoves, int depth, int alpha, int beta, uint8_t *bestMoveId) {
	int score = -20000;
	int bestId = 0;
	int bestIndex = 0;
	nodeCount++;
	Move ttMove = Move{ (uint8_t)0, (uint8_t)0, EMPTY, EMPTY, 0, (uint8_t) NO_ID };
	if (board.inCheck(board.sideToMove)) { // check extension
		depth++;
	}
	tTable.probe(board.zobristKey, depth, alpha, beta, &ttMove);
	sortMoves(moves, numOfMoves, ttMove.id, 0);
	for (int i = 0; i < numOfMoves; i++) {
		if (timer.timesUp == true || stopSearch) {
			*bestMoveId = NO_ID;
			return 0;
		}
		board.moveMake(moves[i]);
		if (board.inCheck(Color(board.sideToMove*(-1)))) { // move that puts its own king in check
			board.moveUnmake();
			continue;
		}

		if ((i == 0) || (-alphaBeta(-alpha - 1, -alpha, depth - 1, 1, ALLOW_NULL, NOT_PV) > alpha)) { // principal variation search
			score = -alphaBeta(-beta, -alpha, depth - 1, 1, ALLOW_NULL, IS_PV);
		}

		board.moveUnmake();
		if (score > alpha) {
			alpha = score;
			*bestMoveId = moves[i].id;
			bestIndex = i;
			if (score >= beta) {
				tTable.saveEntry(board.zobristKey, depth, board.halfMoveCount, beta, TT_BETA, moves[bestIndex]);
				return beta;
			}
			tTable.saveEntry(board.zobristKey, depth, board.halfMoveCount, alpha, TT_ALPHA, moves[bestIndex]);
			infoPV(depth, score);
		}
	}

	tTable.saveEntry(board.zobristKey, depth, board.halfMoveCount, alpha, TT_EXACT, moves[bestIndex]);
	return alpha;
}

inline void Engine::sortMoves(Move *moves, int numOfMoves, int bestMoveId, int ply) {
	int orderingValues[218] = { 0 };
	for (int i = 0; i < numOfMoves; i++) {
		if (bestMoveId != 0 && bestMoveId != NO_ID && moves[i].id == bestMoveId) {
			orderingValues[moves[i].id] += HASH_ORDER;
			continue;
		}
		if (moves[i].flags & MFLAGS_CPT || moves[i].flags & MFLAGS_PROMOTION) {
			if (moves[i].flags & MFLAGS_CPT) {
				if (ply <= SEE_ORDER_DEPTH) {
					orderingValues[moves[i].id] += SEE(moves[i].toSq);
					orderingValues[moves[i].id] += (orderingValues[moves[i].id] > 0) ? CPT_ORDER : 0;
				}
				else {
					orderingValues[moves[i].id] += (pieceValues[moves[i].attackedPiece] - pieceValues[moves[i].movedPiece]) + CPT_ORDER;
				}
			}
			else {
				if (moves[i].flags & MFLAGS_PROMOTION_QUEEN) {
					orderingValues[moves[i].id] += (pieceValues[QUEEN] - pieceValues[PAWN]) + PROMOTION_ORDER;
				}
				else if (moves[i].flags & MFLAGS_PROMOTION_ROOK) {
					orderingValues[moves[i].id] += (pieceValues[ROOK] - pieceValues[PAWN]) + PROMOTION_ORDER;
				}
				else if (moves[i].flags & MFLAGS_PROMOTION_BISHOP) {
					orderingValues[moves[i].id] += (pieceValues[BISHOP] - pieceValues[PAWN]) + PROMOTION_ORDER;
				}
				else orderingValues[moves[i].id] += (pieceValues[KNIGHT] - pieceValues[PAWN]) + PROMOTION_ORDER;
			}
		}
		else {
			if (moves[i].fromSq == killers[ply][KILLER_PRIMARY].fromSq && moves[i].toSq == killers[ply][KILLER_PRIMARY].toSq) {
				orderingValues[moves[i].id] += KILLER_ORDER;
			}
			else if (moves[i].fromSq == killers[ply][KILLER_SECONDARY].fromSq && moves[i].toSq == killers[ply][KILLER_SECONDARY].toSq) {
				orderingValues[moves[i].id] += KILLER_ORDER - 1;
			}
			else {
				orderingValues[moves[i].id] += historyMoves[moves[i].fromSq][moves[i].toSq];
			}
		}
	}
	std::sort(moves, moves + numOfMoves, [&orderingValues](Move lhs, Move rhs) {return orderingValues[lhs.id] > orderingValues[rhs.id]; });
}

int Engine::iterativeDeepening(Move *moves, int numOfMoves) {
	int score;
	Color findMoveFor = board.sideToMove;

	int pieceVal = board.getSideMaterialValue(WHITE) - pieceValues[KING];
	if (pieceVal <= 1200) {
		board.phaseFlag = PHASE_EG;
	}
	pieceVal = board.getSideMaterialValue(BLACK) - pieceValues[KING];
	if (pieceVal <= 1200) {
		board.phaseFlag = PHASE_EG;
	}
	
	int windowMissCount = 0;
	uint8_t newBest = 0;
	uint8_t bestId = 0;
	int alpha = -200000;
	int beta = 200000;
	Move pvMove;
	stopSearch = false;
	nodeCount = 0;
	unsigned long searchLength = calculateTimeForMove(findMoveFor);
	if (wmsLeft != -1) {
		timer.start(true, searchLength);
	}
	unsigned long searchStart;
	unsigned long long lastNodeCount = 0;
	int currDepth;
	decHistoryTable();
	std::cout << "Depth\tNodes\t\tTime\t\tNPS\tScore\tPV" << std::endl;
	for (currDepth = 1; currDepth <= maxDepth; currDepth++) {
		searchStart = timer.mseconds;
		score = findBestMove(moves, numOfMoves, currDepth, alpha, beta, &newBest);
		if (score <= alpha) {
			alpha -= pow(2 * WINDOW_SIZE, windowMissCount + 1);
			currDepth--;
			windowMissCount++;
			continue;
		}
		else if (score >= beta) {
			beta += pow(2 * WINDOW_SIZE, windowMissCount + 1);
			currDepth--;
			windowMissCount++;
			continue;
		}
		windowMissCount = 0;
		alpha = score - WINDOW_SIZE;
		beta = score + WINDOW_SIZE;
		if (newBest == NO_ID) {
			std::cout << "TIMES UP" << std::endl;
			break;
		}
		infoNPS(nodeCount - lastNodeCount, searchStart);
		infoPV(currDepth, INVALID);
		getSearchStats(currDepth, lastNodeCount, searchStart);
		bestId = newBest;
		if (wmsLeft != -1 && (timer.mseconds - searchStart) * DEPTH_TIME_INCREASE > searchLength - timer.mseconds) { // next depth would take longer than remaining time
			std::cout << "Ended search early as next depth would take " << (timer.mseconds - searchStart) * DEPTH_TIME_INCREASE << " ms and we have " << searchLength - timer.mseconds << " ms remaining" << std::endl;
			timer.stop();
			break;
		}
		lastNodeCount = nodeCount;
	}
	infoNPS(nodeCount, 0);
	getSearchStats(-1, 0, 0);

	return bestId;
}

int Engine::SEE(int sq) { // TODO, can check for only legal moves, but this will slow down and might not be necessary as SEE is only used as a guideline
	int wAttackers[20]; // TODO really hoping we never go over 16 here
	int bAttackers[20];

	int numOfwAttackers = board.getSqAttackers(wAttackers, sq, WHITE);
	int numOfbAttackers = board.getSqAttackers(bAttackers, sq, BLACK);

	std::sort(wAttackers, wAttackers + numOfwAttackers, [this](int lhs, int rhs) {return pieceSorting[board.board[lhs]] < pieceSorting[board.board[rhs]]; });
	std::sort(bAttackers, bAttackers + numOfbAttackers, [this](int lhs, int rhs) {return pieceSorting[board.board[lhs]] < pieceSorting[board.board[rhs]]; });

	int pieceAtSq = board.board[sq];
	int sideToMove = board.sideToMove;
	int score = 0;
	int newAttackerSq;
	bool inserted;

	int i = 0;

	while ((sideToMove == WHITE && i < numOfwAttackers) || (sideToMove == BLACK && i < numOfbAttackers)) {
		newAttackerSq = -1;
		if (sideToMove == WHITE && board.board[wAttackers[i]] == KING && numOfbAttackers - i > 0) {
			break; // piece is defended so king cant attack
		}
		else if (sideToMove == BLACK && board.board[bAttackers[i]] == KING && numOfwAttackers - i > 0) {
			break; 
		}
		score += pieceValues[pieceAtSq] * sideToMove * board.sideToMove;
		if (sideToMove == WHITE) {
			pieceAtSq = board.board[wAttackers[i]];
			if (board.board[wAttackers[i]] != KNIGHT) // no piece can be hiding behind a king or knight and still attack sq
				newAttackerSq = board.getSqOfFirstPieceOnRay(wAttackers[i], dirBySquareDiff[sq - wAttackers[i] + 119]);
		}
		else {
			pieceAtSq = board.board[bAttackers[i]];
			if (board.board[bAttackers[i]] != KNIGHT) // no piece can be hiding behind a king or knight and still attack sq
				newAttackerSq = board.getSqOfFirstPieceOnRay(bAttackers[i], dirBySquareDiff[sq - bAttackers[i] + 119]);
		}
		// find hidden attackers
		if (newAttackerSq != -1) {
			inserted = false;
			if (attackArray[sq - newAttackerSq + 128] & attackGroups[board.board[newAttackerSq]]) {
				if (board.boardColor[newAttackerSq] == WHITE) {
					for (int j = (sideToMove != board.sideToMove) ? i + 1 : i; j < numOfwAttackers; j++) {
						if (pieceSorting[board.board[newAttackerSq]] < pieceSorting[board.board[wAttackers[j]]]) {
							for (int k = numOfwAttackers; k > j; k--) {
								wAttackers[k] = wAttackers[k - 1];
							}
							wAttackers[j] = newAttackerSq;
							inserted = true;
							break;
						}
					}

					if (!inserted) { // the current highest value, therefore insert last
						wAttackers[numOfwAttackers] = newAttackerSq;
					}

					numOfwAttackers++;
				}
				else {
					for (int j = (sideToMove != board.sideToMove) ? i + 1 : i; j < numOfbAttackers; j++) {
						if (pieceSorting[board.board[newAttackerSq]] < pieceSorting[board.board[bAttackers[j]]]) {
							for (int k = numOfbAttackers; k > j; k--) {
								bAttackers[k] = bAttackers[k - 1];
							}
							bAttackers[j] = newAttackerSq;
							inserted = true;
							break;
						}
					}

					if (!inserted) { // the current highest value, therefore insert last
						bAttackers[numOfbAttackers] = newAttackerSq;
					}
					numOfbAttackers++;
				}
			}
		}

		sideToMove *= (-1);
		if (sideToMove == board.sideToMove) i++;
	}

	return score;
}

inline void Engine::getSearchStats(int searchDepth, unsigned long long prevNodeCount, unsigned long startTime) {
	if (searchDepth == -1) {
		std::cout << "Total:\t";
	}
	else {
		std::cout << searchDepth << "\t";
	}
	//std::cout << "Search to depth " << searchDepth << " took " << timer.mseconds - startTime << " milliseconds, and searched " << nodeCount - prevNodeCount << " nodes" << std::endl;
	std::cout << nodeCount - prevNodeCount << "\t\t" << timer.mseconds - startTime << "\tms\t";
	infoNPS(nodeCount - prevNodeCount, startTime);
	if (searchDepth != -1) {
		std::cout << "\t";
		infoPV(searchDepth, INVALID);
	}
	std::cout << std::endl;
}

inline unsigned long Engine::calculateTimeForMove(Color side) {
	if (mode == PROTO_UCI) {
			int timeLeft = (side == WHITE) ? wmsLeft : bmsLeft;
			int timeInc = (side == WHITE) ? wTimeInc : bTimeInc;
			if (timeLeft == -1) { //not timed game
				return FIXED_SEARCH_DURATION;
			}
			float fractionAllowed = (timeInc > 0) ? 0.08 : 0.04;
			if (board.halfMoveCount < 20) {
				fractionAllowed = fractionAllowed / 2;
			} // TODO, something with 0 increment
			std::stringstream ss;
			ss << "Gonna use " << timeLeft * fractionAllowed + 0.8*timeInc << " mseconds";
			comSend(ss.str());
			return timeLeft * fractionAllowed + timeInc;
			
	}
	else if (mode == PROTO_NOTHING) {
		return FIXED_SEARCH_DURATION;
	}
	
}

inline void Engine::infoNPS(unsigned long long nodes, unsigned long startTime) {
	unsigned long elapsedTime = timer.mseconds - startTime;
	if (elapsedTime) {
		switch (mode) {
			case PROTO_NOTHING:
				std::cout << (nodes / double((timer.mseconds - startTime))) * 1000;
				break;
			case PROTO_UCI:
				std::stringstream ss;
				ss << "info nps " << (nodes / double((timer.mseconds - startTime))) * 1000;
				comSend(ss.str());
				break;
		}
	}
	else if (mode == PROTO_NOTHING) {
		std::cout << "N/A";
	}
}

void Engine::infoPV(int searchDepth, int score) {
	int pvLength = 0;
	TT_FLAG lastMoveFlag;
	Move pvMove;
	std::string pvString;
	std::stringstream pvSS;
	switch (mode) {
		case PROTO_UCI:
			pvSS << "info depth " << searchDepth << " pv ";
			break;
	}
	for (int j = 0; j < searchDepth; j++) {
		if (!(lastMoveFlag = tTable.getPV(board.zobristKey, &pvMove))) {
			break;
		}
		pvSS << SQ_FILE(pvMove.fromSq) << SQ_RANK(pvMove.fromSq) << SQ_FILE(pvMove.toSq) << SQ_RANK(pvMove.toSq) << " ";
		board.moveMake(pvMove);
		pvLength++;
		//if (lastMoveFlag != TT_EXACT) {
		//	break;
		//}
	}
	if (score == INVALID) {
		switch (mode) {
		case PROTO_NOTHING:
			pvString = std::to_string(evaluatePosition()*board.sideToMove) + "\t" + pvSS.str();
			break;
		case PROTO_UCI:
			pvSS << "score cp " << evaluatePosition()*board.sideToMove;
			break;
		}
	}
	else if (mode == PROTO_UCI) {
		pvSS << "score cp " << score;
	}
	else if (mode == PROTO_NOTHING) {
		pvString = std::to_string(score) + "\t" + pvSS.str();
	}

	for (int j = 0; j < pvLength; j++) {
		board.moveUnmake();
	}

	switch (mode) {
		case PROTO_NOTHING:
			std::cout << pvString;
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
		comSend("id name c++hessDemo");
		comSend("id author Eivindeb");
		comSend("uciok");
	}
	else if (command == "isready") {
		comSend("readyok");
	}
	else if (command == "quit") {
		exit(0);
	}
	else if (command == "stop") {
		stopSearch = true;
	}
	else if (command == "ucinewgame") {
		board = Board();
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
				
				if (sqFrom == board.history[board.historyIndex].move.fromSq && sqTo == board.history[board.historyIndex].move.toSq) {
					return 0; // the move the engine did so we ignore it
				}

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
			if (tokens.size() > 1) {
				for (int i = 1; i < tokens.size(); i++) {
					if (tokens[i] == "infinite") {
						wmsLeft = -1;
						bmsLeft = -1;
						break;
					}
					if (tokens[i] == "wtime") {
						wmsLeft = std::stoi(tokens[i + 1]);
						i++;
					}
					else if (tokens[i] == "btime") {
						bmsLeft = std::stoi(tokens[i + 1]);
						i++;
					}
					else if (tokens[i] == "winc") {
						wTimeInc = std::stoi(tokens[i + 1]);
						i++;
					}
					else if (tokens[i] == "binc") {
						bTimeInc = std::stoi(tokens[i + 1]);
						i++;
					}
				}
			}
			Move moves[218];
			int numOfMoves;
			int index;
			std::stringstream moveStream;
			if (board.inCheck(board.sideToMove)) {
				numOfMoves = board.getLegalMovesInCheck(moves);
			}
			else {
				numOfMoves = board.getLegalMoves(moves);
			}
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