#include "stdafx.h"
#include "Engine.h"
#include <iostream>
#include <algorithm>


int Engine::alphaBeta(int alpha, int beta, int depthLeft) {
	if (depthLeft == 0) return quiescence(alpha, beta);
	Move moves[218];
	Move ttMove = Move{ (uint8_t) 0, (uint8_t) 0, EMPTY, EMPTY, 0, (uint8_t) -1 };
	int numOfMoves = 0;
	if (board.inCheck(board.sideToMove)) {
		numOfMoves = board.getLegalMovesInCheck(moves);
		if (numOfMoves == 0) {
			return (MATE_SCORE + (maxDepth - depthLeft))*board.sideToMove;
		}
	}
	else {
		numOfMoves = board.getLegalMoves(moves);
	}

	int score = 0;
	int posValBefore = 0;
	int posValAfter = 0;
	int bestMoveIndex = 0;
	int ttVal = 0;
	TT_FLAG ttFlag = TT_ALPHA;
	unsigned long long zobristBefore = 0;
	if (tTable.probe(board.zobristKey, depthLeft, alpha, beta, &ttMove) != INVALID) {
		return ttVal;
	}
	sortMoves(moves, numOfMoves, ttMove.id);
	for (int i = 0; i < numOfMoves; i++) {
		posValBefore = board.positionTotal;
		zobristBefore = board.zobristKey;
		/*
		for (int p = 0; p < 7; p++) {
			numOfPieces[p] = 0;
			numOfPiecesAfter[p] = 0;
		}
		for (int sq = 0; sq < 120; sq++) {
			if (ON_BOARD(sq)) {
				numOfPieces[board.board[sq]]++;
			}
		}*/
		board.moveMake(moves[i]);
		if (board.inCheck(Color(board.sideToMove*(-1)))) { // move that puts its own king in check
			board.moveUnmake();
			continue;
		}
		score = -alphaBeta(-beta, -alpha, depthLeft - 1);
		board.moveUnmake();
		posValAfter = board.positionTotal;
		if (posValAfter != posValBefore) {
			posValAfter = 0;
		}
		if (board.zobristKey != zobristBefore) {
			zobristBefore = 0;
		}
		/*for (int sq = 0; sq < 120; sq++) {
			if (ON_BOARD(sq)) {
				numOfPiecesAfter[board.board[sq]]++;
			}
		}
		for (int p = 0; p < 7; p++) {
			if (numOfPieces[p] != numOfPiecesAfter[p]) {
				std::cout << "shieeet " << p << std::endl;
			}
		}*/
		if (score >= beta) {
			tTable.saveEntry(board.zobristKey, depthLeft, board.halfMoveCount, beta, TT_BETA, moves[i]);
			return beta;
		}
		if (score > alpha) {
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
unsigned long long Engine::miniMax(int depthLeft) {
	if (depthLeft == 0) return 1;
	unsigned long long nodes = 0;
	unsigned long long nodesLast = 0;

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
		board.moveMake(moves[i]);
		if (!board.inCheck(Color(board.sideToMove*(-1)))) {
			nodes += miniMax(depthLeft - 1);
			if (maxDepth == depthLeft) {
				std::cout << SQ_FILE(moves[i].fromSq) << SQ_RANK(moves[i].fromSq) << " " << SQ_FILE(moves[i].toSq) << SQ_RANK(moves[i].toSq) << "\t" << nodes - nodesLast << std::endl;
				nodesLast = nodes;
			}
		}
		board.moveUnmake();
	}

	if (maxDepth == depthLeft) {
		std::cout << "Total nodes: " << nodes << std::endl;
	}

	return nodes;
}

inline void Engine::mvvLva(Move *moves, int numOfMoves) {
	std::sort(moves, moves + numOfMoves, [](Move &lhs, Move &rhs) {int attackerLhs = (lhs.movedPiece == KING) ? 6 : lhs.movedPiece;
																	int attackerRhs = (rhs.movedPiece == KING) ? 6 : rhs.movedPiece;
																		return lhs.attackedPiece > rhs.attackedPiece || 
																		(lhs.attackedPiece == rhs.attackedPiece && attackerLhs < attackerRhs); });
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

	return (board.materialTotal + mobilityWeight * (wNumOfMoves - bNumOfMoves) + board.positionTotal) * board.sideToMove;
}

int Engine::findBestMove(Move *moves, int numOfMoves, int depth, int alpha, int beta) {
	int score = -10000 * sideToPlay;
	int prevScore = score;
	int bestIndex = 0;
	Move ttMove = Move{ (uint8_t)0, (uint8_t)0, EMPTY, EMPTY, 0, (uint8_t)-1 };
	tTable.probe(board.zobristKey, depth, alpha, beta, &ttMove);
	sortMoves(moves, numOfMoves, ttMove.id);
	for (int i = 0; i < numOfMoves; i++) {
		board.moveMake(moves[i]);
		if (board.inCheck(Color(board.sideToMove*(-1)))) { // move that puts its own king in check
			board.moveUnmake();
			continue;
		}
		score = -alphaBeta(-beta, -alpha, depth-1)*sideToPlay;
		board.moveUnmake();
		std::cout << std::endl << "On move: " << i << "/" << numOfMoves << ":\t" << moves[i].fromSq << " " << moves[i].toSq;
		//std::cout << "\t(" << SQ_FILE(moves[i].fromSq) << SQ_RANK(moves[i].fromSq) << " " << SQ_FILE(moves[i].toSq) << SQ_RANK(moves[i].toSq) << ")\t" << score << std::endl << std::endl;
		if ((sideToPlay == WHITE && score > prevScore) || (sideToPlay == BLACK && score < prevScore)) {
			prevScore = score;
			bestIndex = i;
		}
	}

	std::cout << "Highest score: " << prevScore << std::endl;
	tTable.saveEntry(board.zobristKey, depth, board.halfMoveCount, prevScore, TT_EXACT, moves[bestIndex]);
	return bestIndex;
}

inline void Engine::sortMoves(Move *moves, int numOfMoves, uint8_t bestMoveId) {
	if (bestMoveId != -1) {
		std::swap(moves[0], moves[bestMoveId]);
		mvvLva(moves + 1, numOfMoves - 1); // TODO, might not work
	}
	else {
		mvvLva(moves, numOfMoves);
	}
	
}

int Engine::iterativeDeepening() {
	Move moves[218];
	int numOfMoves = 0;
	int bestMoveIndex = 0;
	int score;
	Move pvMove;
	if (board.inCheck(board.sideToMove)) {
		numOfMoves = board.getLegalMovesInCheck(moves);
	}
	else {
		numOfMoves = board.getLegalMoves(moves);
	}
	std::cout << "PV: " << std::endl;
	for (int i = 1; i <= maxDepth; i++) {
		bestMoveIndex = findBestMove(moves, numOfMoves,i , 200000, -200000);
		for (int j = 0; j < i; j++) {
			score = tTable.probe(board.zobristKey, 0, 0, 0, &pvMove);
			std::cout << pvMove.fromSq << " " << pvMove.toSq;
			std::cout << "\t(" << SQ_FILE(pvMove.fromSq) << SQ_RANK(pvMove.fromSq) << " " << SQ_FILE(pvMove.toSq) << SQ_RANK(pvMove.toSq) << ")\t" << score << std::endl;
			board.moveMake(pvMove);
		}
		for (int j = 0; j < i; j++) {
			board.moveUnmake();
		}
	}

	return bestMoveIndex;
}