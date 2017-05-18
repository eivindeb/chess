#include "stdafx.h"
#include "Evaluator.h"
#include "Board.h"

Evaluator::Evaluator(int tableSize) : evalTable(tableSize) {}

int Evaluator::evaluatePosition(Board *board) {
	int score;
	if ((score = evalTable.probe(board->zobristKey)) != INVALID) {
		return score;
	}
	int moves[218];
	int wNumOfMoves;
	int bNumOfMoves;
	if (board->sideToMove == WHITE) {
		wNumOfMoves = (board->inCheck(WHITE)) ? board->getLegalMovesInCheck(moves) : board->getLegalMoves(moves);
		board->sideToMove = Color(board->sideToMove*(-1));
		bNumOfMoves = (board->inCheck(BLACK)) ? board->getLegalMovesInCheck(moves) : board->getLegalMoves(moves);
	}
	else {
		bNumOfMoves = (board->inCheck(BLACK)) ? board->getLegalMovesInCheck(moves) : board->getLegalMoves(moves);
		board->sideToMove = Color(board->sideToMove*(-1));
		wNumOfMoves = (board->inCheck(WHITE)) ? board->getLegalMovesInCheck(moves) : board->getLegalMoves(moves);
	}
	board->sideToMove = Color(board->sideToMove*(-1)); // mobility slows down evaluation A LOT.
	score = board->materialTotal + board->positionTotal + mobilityWeight * (wNumOfMoves - bNumOfMoves);
	if (board->pieceLists[(board->sideToMove == WHITE) ? BISHOP + 6 : BISHOP][COUNT] > 1) {
		score += pieceValues[BISHOP] * 0.1;
	}
	if (board->pieceLists[(board->sideToMove == WHITE) ? BISHOP : BISHOP + 6][COUNT] > 1) {
		score -= pieceValues[BISHOP] * 0.1;
	}
	int wPawnCount = board->pieceLists[PAWN + 6][COUNT];
	int bPawnCount = board->pieceLists[PAWN][COUNT];
	score += (knightPawnCountEval[wPawnCount] - knightPawnCountEval[bPawnCount] + rookPawnCountEval[wPawnCount] - rookPawnCountEval[bPawnCount]) * board->sideToMove;

	score *= board->sideToMove;

	evalTable.save(board->zobristKey, score);

	return score;
}