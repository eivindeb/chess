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
	historyIndex = -1;
	materialTotal = 0;
	wCastlingRights = CASTLE_BOTH;
	bCastlingRights = CASTLE_BOTH;
}

int Board::getLegalMoves(Move *moves) {
	int movesInPosition = 0;
	int newPos;

	if (sideToMove == WHITE) {
		if ((wCastlingRights & CASTLE_BOTH) || (wCastlingRights & CASTLE_SHORT)) {
			move_push_back(moves, movesInPosition++, 4, 6, KING, 0, MFLAGS_CASTLE);
		}
		if ((wCastlingRights & CASTLE_BOTH) || (wCastlingRights & CASTLE_LONG)) {
			move_push_back(moves, movesInPosition++, 4, 2, KING, 0, MFLAGS_CASTLE);
		}
	}
	else {
		if ((bCastlingRights & CASTLE_BOTH) || (bCastlingRights & CASTLE_SHORT)) {
			move_push_back(moves, movesInPosition++, 116, 118, KING, 0, MFLAGS_CASTLE);
		}
		if ((bCastlingRights & CASTLE_BOTH) || (bCastlingRights & CASTLE_LONG)) {
			move_push_back(moves, movesInPosition++, 116, 114, KING, 0, MFLAGS_CASTLE);
		}
	}

	for (int sq = 0; sq < 120; sq++) {
		if (board[sq] != EMPTY && boardColor[sq] == sideToMove) {  // a players piece on this square
			if (board[sq] == PAWN) {  // TODO might have to check for out of bounds here but i dont think it is necessary
				int dirMod = (sideToMove == WHITE) ? 0 : 4;
				if ((sideToMove == WHITE && sq / 16 == 1) || (sideToMove == BLACK && sq / 16 == 6)) {
					if (board[sq + pieceDeltas[PAWN][dirMod + 3]] == EMPTY) {
						move_push_back(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 3], PAWN, 0, MFLAGS_PAWN_DOUBLE);
					}
				}
				if (boardColor[sq + pieceDeltas[PAWN][dirMod]] == (sideToMove*(-1))) {
					move_push_back(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod], PAWN, board[sq + pieceDeltas[PAWN][dirMod]], MFLAGS_CPT);
				}
				if (board[sq + pieceDeltas[PAWN][dirMod + 1]] == EMPTY) {
					move_push_back(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 1], PAWN, 0, 0);
				}
				if (boardColor[sq + pieceDeltas[PAWN][dirMod + 2]] == (sideToMove*(-1))) {
					move_push_back(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 2], PAWN, board[sq + pieceDeltas[PAWN][dirMod + 2]], MFLAGS_CPT);
				}
				if (enPassant != -1) {
					if ((sq + pieceDeltas[PAWN][dirMod]) == enPassant) {
						move_push_back(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod], PAWN, PAWN, MFLAGS_CPT + MFLAGS_ENP);
					}
					else if (sq + pieceDeltas[PAWN][dirMod + 2] == enPassant) {
						move_push_back(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 2], PAWN, PAWN, MFLAGS_CPT + MFLAGS_ENP);
					}
				}
				
			}
			else if (board[sq] <= 2) { // non sliding piece
				for (int dir = 0; dir < 8; dir++) {
					newPos = sq + pieceDeltas[board[sq]][dir];
					if ((newPos & 0x88) == 0) {
						if (boardColor[newPos] == sideToMove*(-1)) {
							move_push_back(moves, movesInPosition++, sq, newPos, board[sq], board[newPos], MFLAGS_CPT);
						}
						else if (board[newPos] == EMPTY) {
							move_push_back(moves, movesInPosition++, sq, newPos, board[sq], 0, 0);
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
								move_push_back(moves, movesInPosition++, sq, newPos, board[sq], board[newPos], MFLAGS_CPT);
							}
							break;
						}
						move_push_back(moves, movesInPosition++, sq, newPos, board[sq], 0, 0);
					}
				}
			}
		}
	}

	return movesInPosition;
}

void Board::moveMake(Move move) {
	if (move.flags & MFLAGS_CPT) {
		materialTotal += (pieceValues[move.attackedPiece] * sideToMove);
	}
	history[++historyIndex] = State{move, wCastlingRights, bCastlingRights, enPassant};
	board[move.toSq] = board[move.fromSq];
	boardColor[move.toSq] = boardColor[move.fromSq];
	board[move.fromSq] = EMPTY;
	boardColor[move.fromSq] = NONE;
	if (move.flags & MFLAGS_ENP) {
		if (abs(move.fromSq - move.toSq) == NW) {
			board[move.fromSq + WEST] = EMPTY;
			boardColor[move.fromSq + WEST] = NONE;
		}
		else {
			board[move.fromSq + EAST] = EMPTY;
			boardColor[move.fromSq + EAST] = NONE;
		}
	}
	if (move.flags & MFLAGS_PAWN_DOUBLE) {
		enPassant = (move.fromSq + move.toSq) / 2;
	} else if (enPassant != -1) {
		enPassant = -1;
	}

	halfMoveCount++;
	sideToMove = Color(sideToMove*(-1));
}

void Board::moveUnmake() {
	State prevState = history[historyIndex--];
	sideToMove = Color(sideToMove*(-1));
	if (prevState.move.flags & MFLAGS_CPT) {
		materialTotal -= (pieceValues[prevState.move.attackedPiece] * sideToMove);
		if (prevState.move.flags & MFLAGS_ENP) {
			if (abs(prevState.move.fromSq - prevState.move.toSq) == NW) {
				board[prevState.move.fromSq + WEST] = PAWN;
				boardColor[prevState.move.fromSq + WEST] = Color(sideToMove*(-1));
			}
			else {
				board[prevState.move.fromSq + EAST] = PAWN;
				boardColor[prevState.move.fromSq + EAST] = Color(sideToMove*(-1));
			}
			board[prevState.move.toSq] = EMPTY;
			boardColor[prevState.move.toSq] = NONE;
		}
		else {
			board[prevState.move.toSq] = Piece(prevState.move.attackedPiece);
			boardColor[prevState.move.toSq] = Color(sideToMove*(-1));
		}
	}
	else {
		board[prevState.move.toSq] = EMPTY;
		boardColor[prevState.move.toSq] = NONE;
	}
	board[prevState.move.fromSq] = Piece(prevState.move.movedPiece);
	boardColor[prevState.move.fromSq] = sideToMove;

	wCastlingRights = prevState.wCastleRights;
	bCastlingRights = prevState.bCastleRights;
	enPassant = prevState.enPassant;
	halfMoveCount--;
}

void Board::printMoves(Move *moves, int numOfMoves) {
	std::cout << "Number of moves: " << numOfMoves << std::endl;
	for (int i = 0; i < numOfMoves; i++) {
		std::cout << std::to_string(i) << ".\t" << char((moves[i].fromSq % 8) + 97) << (moves[i].fromSq >> 4) + 1 << " " << char((moves[i].toSq % 8) + 97) << (moves[i].toSq >> 4) + 1;
		if (moves[i].flags & MFLAGS_CPT) {
			std::cout << "\tCapture";
		}
		std::cout << std::endl;
	}
}

void Board::move_push_back(Move *moves, int moveNum, int squareFrom, int squareTo, int movedPiece, int attacked, int flags) {
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