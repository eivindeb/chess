#include "Board.h"

Board::Board() {
	Piece backRow[8] = { ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK };
	for (int i = 0; i < 128; i++) {
		if (i <= 7) {
			board[i] = backRow[i];
			boardColor[i] = WHITE;
		}
		else if (i >= 112 && i <= 119) {
			board[i] = backRow[i - 112];
			boardColor[i] = BLACK;
		}
		else {
			board[i] = EMPTY;
			boardColor[i] = NONE;
		}
	}

	sideToMove = WHITE;
	halfMoveCount = 0;
}

int Board::getLegalMoves(int *moves) {
	int movesInPosition = 0;

	for (int sq = 0; sq < 128; sq++) {
		if (board[sq] != EMPTY && boardColor[sq] == sideToMove) {  // a players piece on this square
			switch (board[sq]) {
				case PAWN:
					if (board[sq + 8] == EMPTY) {
						int doubleMove = sideToMove == WHITE ? 3 : 7;
						if (board[sq + doubleMove] == EMPTY) {
							move_push_back(moves, movesInPosition++, sq, sq + doubleMove, 0, 0, 0);
						}
						 // TODO maybe not do this, the idea is that i fill the square so that the double move is no longer accessible
					}
					if (boardColor[sq + piece_deltas[PAWN][0]] == (sideToMove*(-1))) {
						move_push_back(moves, movesInPosition++, sq, sq + piece_deltas[PAWN][0], 1, board[sq + piece_deltas[PAWN][0]], board[sq]);
					}
			}
		}
	}
}

void Board::move_push_back(int *moves, int moveNum, int squareFrom, int squareTo, int capture, int attacked, int attacker) {
	if (capture) {
		moves[moveNum] = squareFrom + ((squareTo) << 7) + (capture << 14) + (attacked << 15) + (attacker << 18);
	}
	else {
		moves[moveNum] = squareFrom + ((squareTo) << 7);
	}
}