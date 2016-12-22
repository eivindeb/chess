#include "stdafx.h"
#include "Board.h"
#include <iostream>

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

	for (int i = 0; i < 8; i++) {
		board[i + 16] = PAWN;
		boardColor[i + 16] = WHITE;
		board[i + 96] = PAWN;
		boardColor[i + 96] = BLACK;
	}

	sideToMove = WHITE;
	halfMoveCount = 0;
	for (int i = 0; i < 24; i++) {
		enPassant[i] = -1;
	}
}

int Board::getLegalMoves(int *moves) {
	int movesInPosition = 0;
	int newPos;

	for (int sq = 0; sq < 128; sq++) {
		if (board[sq] != EMPTY && boardColor[sq] == sideToMove) {  // a players piece on this square
			if (board[sq] == PAWN) {  // TODO might have to check for out of bounds here but i dont think it is necessary
				if (enPassant[8 * sideToMove + sq % 8 + 8] == -1) {
					int doubleMove = (sideToMove == WHITE) ? 3 : 7;
					if (board[sq + pieceDeltas[PAWN][doubleMove]] == EMPTY) {
						move_push_back(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][doubleMove], 0, 0, 0);
					}
					enPassant[8 * sideToMove + sq % 8 + 8] = halfMoveCount;
				}
				if (boardColor[sq + pieceDeltas[PAWN][0]] == (sideToMove*(-1))) {
					move_push_back(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][0], 1, board[sq + pieceDeltas[PAWN][0]], board[sq]);
				}
				if (board[sq + pieceDeltas[PAWN][1]] == EMPTY) {
					move_push_back(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][1], 0, 0, 0);
				}
				if (boardColor[sq + pieceDeltas[PAWN][2]] == (sideToMove*(-1))) {
					move_push_back(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][2], 1, board[sq + pieceDeltas[PAWN][2]], board[sq]);
				}
			}
			else if (board[sq] <= 2) { // non sliding piece
				for (int dir = 0; dir < 8; dir++) {
					newPos = sq + pieceDeltas[board[sq]][dir];
					if ((newPos & 0x88) == 0) {
						if (boardColor[newPos] == sideToMove*(-1)) {
							move_push_back(moves, movesInPosition++, sq, newPos, 1, board[sq], board[newPos]);
						}
						else if (board[newPos] == EMPTY) {
							move_push_back(moves, movesInPosition++, sq, newPos, 0, 0, 0);
						}
					}
				}
			}
			else {
				for (int dir = 0; dir < 8; dir++) {
					if (pieceDeltas[board[sq]][dir] == 0) break;
					newPos = sq;
					while (((newPos += pieceDeltas[board[sq]][dir]) & 0x88) == 0) {
						if (board[newPos] != EMPTY) {
							if (boardColor[newPos] == sideToMove*(-1)) {
								move_push_back(moves, movesInPosition++, sq, newPos, 1, board[sq], board[newPos]);
							}
							break;
						}
						move_push_back(moves, movesInPosition++, sq, newPos, 0, 0, 0);
					}
				}
			}
		}
	}

	return movesInPosition;
}

void Board::printMoves(int *moves, int numOfMoves) {
	std::cout << "Number of moves: " << numOfMoves << std::endl;
	for (int i = 0; i < numOfMoves; i++) {
		std::cout << "og: " << moves[i] << "\t";
		int initialSq = (moves[i] & 0x7F);
		int targetSq = ((moves[i] & (0x7F << 7)) >> 7);
		std::cout << char((initialSq % 8) + 97) << (initialSq >> 4) + 1 << " " << char((targetSq % 8) + 97) << (targetSq >> 4) + 1;
		if (moves[i] & (1 << 14)) {
			std::cout << "\tCapture";
		}
		std::cout << std::endl;
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

void Board::printBoard() {
	for (int sq = 0; sq < 128; sq++) {
		if ((sq & 0x88) == 0) {
			if (board[sq] == EMPTY) {
				std::cout << "0 ";
			}
			else if (board[sq] == KING) {
				std::cout << "6 ";
			}
			else {
				std::cout << board[sq] << " ";
			}
		}
		if ((sq % 8) == 7) {
			std::cout << std::endl;
		}
	}
}

std::string Board::getFenString() {
	std::string fen = "";
	int emptyCount = 0;
	for (int sq = 0; sq < 128; sq++) {
		switch (board[sq]) {
			case PAWN:
				if (boardColor[sq] == WHITE) {
					fen += "P";
				}
				else {
					fen += "p";
				}
				break;
			case KING:
				if (boardColor[sq] == WHITE) {
					fen += "K";
				}
				else {
					fen += "k";
				}
				break;
			case KNIGHT:
				if (boardColor[sq] == WHITE) {
					fen += "N";
				}
				else {
					fen += "n";
				}
				break;
			case BISHOP:
				if (boardColor[sq] == WHITE) {
					fen += "B";
				}
				else {
					fen += "b";
				}
				break;
			case ROOK:
				if (boardColor[sq] == WHITE) {
					fen += "R";
				}
				else {
					fen += "r";
				}
				break;
			case QUEEN:
				if (emptyCount != 0) {
				}
				if (boardColor[sq] == WHITE) {
					fen += "Q";
				}
				else {
					fen += "q";
				}
				break;
			case EMPTY:
				emptyCount++;
				break;
		}
	}
	return fen;
}