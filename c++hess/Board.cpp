#include "stdafx.h"
#include "Board.h"
#include <iostream>
#include <string>
#include <stdlib.h>

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
	enPassant = -1;
	for (int i = 0; i < 20; i++) {
		history[i] = 0;
	}
	historyIndex = -1;
	wCastlingRights = CASTLE_BOTH;
	bCastlingRights = CASTLE_BOTH;
}

int Board::getLegalMoves(Move *moves) {
	int movesInPosition = 0;
	int newPos;

	for (int sq = 0; sq < 120; sq++) {
		if (board[sq] != EMPTY && boardColor[sq] == sideToMove) {  // a players piece on this square
			if (board[sq] == PAWN) {  // TODO might have to check for out of bounds here but i dont think it is necessary
				int dirMod = (sideToMove == WHITE) ? 0 : 4;
				if ((sideToMove == WHITE && sq / 16 == 1) || (sideToMove == BLACK && sq / 16 == 6)) {
					if (board[sq + pieceDeltas[PAWN][dirMod + 3]] == EMPTY) {
						move_push_back(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 3], 0, board[sq], 0);
					}
				}
				if (boardColor[sq + pieceDeltas[PAWN][dirMod]] == (sideToMove*(-1))) {
					move_push_back(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod], 1, board[sq], board[sq + pieceDeltas[PAWN][dirMod]]);
				}
				if (board[sq + pieceDeltas[PAWN][dirMod + 1]] == EMPTY) {
					move_push_back(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 1], 0, board[sq], 0);
				}
				if (boardColor[sq + pieceDeltas[PAWN][dirMod + 2]] == (sideToMove*(-1))) {
					move_push_back(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 2], 1, board[sq], board[sq + pieceDeltas[PAWN][dirMod + 2]]);
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
							move_push_back(moves, movesInPosition++, sq, newPos, 0, board[sq], 0);
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
						move_push_back(moves, movesInPosition++, sq, newPos, 0, board[sq], 0);
					}
				}
			}
		}
	}

	return movesInPosition;
}

void Board::moveMake(Move move) {
	int initSq = (move & 0x7F);
	int tarSq = ((move & 0x3f80) >> 7);
	int capture = (move) & MFLAGS_CPT;
	if (capture) {
		materialTotal += (pieceValues[(move & (0b111 << 15)) >> 15] * sideToMove);
	}
	history[++historyIndex] = move | abs((enPassant << 26)) | (wCastlingRights << 34) | (bCastlingRights << 37);
	board[tarSq] = board[initSq];
	boardColor[tarSq] = boardColor[initSq];
	board[initSq] = EMPTY;
	boardColor[initSq] = NONE;

	if (enPassant != -1) {
		enPassant = -1;
	}

	halfMoveCount++;
	sideToMove = Color(sideToMove*(-1));
}

void Board::moveUnmake() {
	int move = history[historyIndex--];
	sideToMove = Color(sideToMove*(-1));
	int initSq = (move & 0x7F);
	int tarSq = ((move & 0x3f80) >> 7);
	int capture = (move) & MFLAGS_CPT;
	if (capture) {
		int capturedPiece = (move & (0b111 << 15)) >> 15;
		materialTotal -= (pieceValues[capturedPiece] * sideToMove);
		board[tarSq] = Piece(capturedPiece);
		boardColor[tarSq] = Color(sideToMove*(-1));
	}
	else {
		board[tarSq] = EMPTY;
		boardColor[tarSq] = NONE;
	}
	board[initSq] = Piece((move & (0b111 << 15)) >> 15);
	boardColor[initSq] = sideToMove;

	wCastlingRights = (move & (0b111 << 34)) >> 34;
	bCastlingRights = (move & (0b111 << 37)) >> 37;
	enPassant = (move & (0xFF << 26)) >> 26;
	halfMoveCount--;
}

void Board::printMoves(Move *moves, int numOfMoves) {
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

void Board::move_push_back(Move *moves, int moveNum, int squareFrom, int squareTo, int flags, int movedPiece, int attacked) {
	moves[moveNum] = Move{ squareFrom, squareTo, movedPiece, attacked, flags };
}

void Board::printBoard() {
	int sq = 112;
	while(sq >= 0){
		if (board[sq] == EMPTY) {
			std::cout << "0  ";
		}
		else if (board[sq] == KING) {
			if (boardColor[sq] == -1) {
				std::cout << "*6 ";
			}
			else {
				std::cout << "6  ";
			}
		}
		else {
			if (boardColor[sq] == -1) {
				std::cout << "*" << board[sq] << " ";
			}
			else {
				std::cout << board[sq] << "  ";
			}	
		}
		sq++;
		if ((sq & 0x88) != 0) {
			std::cout << std::endl;
			sq -= 24;
		}
	}
}

std::string Board::getFenString() {
	std::string fen = "";
	int emptyCount = 0;
	int sq = 112;
	while(sq >= 0) {
		if (board[sq] != EMPTY) {
			if (emptyCount != 0) {
				fen += std::to_string(emptyCount);
				emptyCount = 0;
			}
		}
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
		sq++;
		if ((sq & 0x88) != 0) {
			if (emptyCount != 0) {
				fen += std::to_string(emptyCount);
				emptyCount = 0;
			}
			if (sq > 8) {
				fen += "/";
			}
			sq -= 24;
		}
	}
	fen += (sideToMove == WHITE) ? " w" : " b";

	// TODO fix this (castling)
	fen += " KQkq";
	if (enPassant != -1) {
		fen += char((enPassant % 8) + 97) + std::to_string((enPassant / 16) + 1);
	}
	else {
		fen += " -";
	}

	// TODO fix this, should be number of half moves since last pawn advancement or capture
	fen += " 0 ";

	fen += std::to_string(int(halfMoveCount / 2) + 1);

	return fen;
}