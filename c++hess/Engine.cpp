#include "stdafx.h"
#include "Engine.h"
#include <iostream>
#include <algorithm>

Engine::Engine(int _sideToPlay, int _depth, std::string fen) {
	sideToPlay = _sideToPlay;
	depth = _depth;

	if (fen == "") board = Board();
	else board = Board(fen);
}

int Engine::alphaBeta(int alpha, int beta, int depthLeft) {
	if (depthLeft == 0) return quiescence(alpha, beta);

	int score = 0;
	Move moves[218];
	int posValBefore = 0;
	int posValAfter = 0;
	//int numOfPieces[7] = { 0, 0, 0, 0, 0, 0, 0 };
	//int numOfPiecesAfter[7] = { 0, 0, 0, 0, 0, 0, 0 };
	int numOfMoves = board.getLegalMoves(moves);
	for (int i = 0; i < numOfMoves; i++) {
		posValBefore = board.positionTotal;
		/*if (i == 0 && depthLeft == 2 && numOfMoves == 28 && alpha == -500 && beta == 200000 && board.halfMoveCount == 8) {
			numOfPieces[0] = 0;
		}
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
		if (posValAfter != posValBefore && depthLeft == 1) {
			posValAfter = 0;
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
			return beta;
		}
		if (score > alpha) {
			alpha = score;
		}
	}

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
	int numOfCaptures = board.getCaptureMoves(moves);
	mvvLva(moves, numOfCaptures);
	for (int i = 0; i < numOfCaptures; i++) {
		if (!(board.posFlag & POS_EG) && !(moves[i].flags & MFLAGS_PROMOTION) && standPat + pieceValues[moves[i].attackedPiece] + 200 < alpha) { // delta pruning
			continue;
		}
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

int Engine::findBestMove(Move *moves, int numOfMoves) {
	int score = -10000 * sideToPlay;
	int prevScore = score;
	int bestIndex = 0;
	for (int i = 0; i < numOfMoves; i++) {
		board.moveMake(moves[i]);
		if (board.inCheck(Color(board.sideToMove*(-1)))) { // move that puts its own king in check
			board.moveUnmake();
			continue;
		}
		score = -alphaBeta(-200000, 200000, depth-1)*sideToPlay;
		board.moveUnmake();
		std::cout << std::endl << "On move: " << i << "/" << numOfMoves << ":\t" << moves[i].fromSq << " " << moves[i].toSq;
		std::cout << "\t(" << SQ_FILE(moves[i].fromSq) << SQ_RANK(moves[i].fromSq) << " " << SQ_FILE(moves[i].toSq) << SQ_RANK(moves[i].toSq) << ")\t" << score << std::endl << std::endl;
		if ((sideToPlay == WHITE && score > prevScore) || (sideToPlay == BLACK && score < prevScore)) {
			prevScore = score;
			bestIndex = i;
		}
	}

	std::cout << "Highest score: " << prevScore << std::endl;
	return bestIndex;
}