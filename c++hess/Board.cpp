#include "stdafx.h"
#include "Board.h"
#include <iostream>
#include <string>
#include <stdlib.h>
#include <algorithm>
#include <iterator>
#include <sstream>

Board::Board(std::string fen) {
	if (fen == "false") {
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
		materialTotal = 0;
		wCastlingRights = CASTLE_BOTH;
		bCastlingRights = CASTLE_BOTH;
		wKingSq = 4;
		bKingSq = 116;
	}
	else {
		std::stringstream ss;
		ss.str(fen);
		std::string item;
		std::string fenSplit[6];
		int index = 0;
		while (std::getline(ss, item, ' ')) {
			fenSplit[index++] = item;
		}

		sideToMove = (fenSplit[1] == "w") ? WHITE : BLACK;
		wCastlingRights = 0;
		bCastlingRights = 0;

		for (char& c : fenSplit[2]) {
			switch (c) {
				case 'K':
					wCastlingRights += CASTLE_SHORT;
					break;
				case 'k':
					bCastlingRights += CASTLE_SHORT;
					break;
				case 'Q':
					wCastlingRights += CASTLE_LONG;
					break;
				case 'q':
					bCastlingRights += CASTLE_LONG;
					break;
				case '-': // not actually necessary but o'well
					wCastlingRights = 0;
					bCastlingRights = 0;
			}
		}

		if (fenSplit[3] == "-") {
			enPassant = -1;
		}
		else {
			char file = fenSplit[3].at(0);
			int rank = (fenSplit[3].at(1) - '0');
			enPassant = (int(file) - 97) + (rank - 1) * 16;
		}

		halfMoveClk = std::stoi(fenSplit[4]);

		halfMoveCount = (std::stoi(fenSplit[5]) * 2);
		if (sideToMove == BLACK) halfMoveCount++;

		materialTotal = 0;

		int sq = 112;
		for (char& c : fenSplit[0]) {
			switch (c) {
				case 'P':
					board[sq] = PAWN;
					boardColor[sq] = WHITE;
					materialTotal += pieceValues[PAWN];
					break;
				case 'p':
					board[sq] = PAWN;
					boardColor[sq] = BLACK;
					materialTotal -= pieceValues[PAWN];
					break;
				case 'N':
					board[sq] = KNIGHT;
					boardColor[sq] = WHITE;
					materialTotal += pieceValues[KNIGHT];
					break;
				case 'n':
					board[sq] = KNIGHT;
					boardColor[sq] = BLACK;
					materialTotal -= pieceValues[KNIGHT];
					break;
				case 'B':
					board[sq] = BISHOP;
					boardColor[sq] = WHITE;
					materialTotal += pieceValues[BISHOP];
					break;
				case 'b':
					board[sq] = BISHOP;
					boardColor[sq] = BLACK;
					materialTotal -= pieceValues[BISHOP];
					break;
				case 'R':
					board[sq] = ROOK;
					boardColor[sq] = WHITE;
					materialTotal += pieceValues[ROOK];
					break;
				case 'r':
					board[sq] = ROOK;
					boardColor[sq] = BLACK;
					materialTotal -= pieceValues[ROOK];
					break;
				case 'Q':
					board[sq] = QUEEN;
					boardColor[sq] = WHITE;
					materialTotal += pieceValues[QUEEN];
					break;
				case 'q':
					board[sq] = QUEEN;
					boardColor[sq] = BLACK;
					materialTotal -= pieceValues[QUEEN];
					break;
				case 'K':
					board[sq] = KING;
					boardColor[sq] = WHITE;
					materialTotal += pieceValues[KING];
					wKingSq = sq;
					break;
				case 'k':
					board[sq] = KING;
					boardColor[sq] = BLACK;
					materialTotal -= pieceValues[KING];
					bKingSq = sq;
					break;
				case '/':
					sq -= 25;
					break;
				default: // any number of empty squares
					for (int i = 0; i < int(c - '0'); i++) {
						board[sq] = EMPTY;
						boardColor[sq] = NONE;
						sq++;
					}
					sq--;
			}
			sq++;
		}
	}
	historyIndex = -1;
}

int Board::getLegalMoves(Move *moves) {
	int movesInPosition = 0;
	int newPos;

	if (sideToMove == WHITE) {
		if ((wCastlingRights & CASTLE_BOTH) || (wCastlingRights & CASTLE_SHORT)) {
			if (board[4 + EAST] == EMPTY && board[4 + 2 * EAST] == EMPTY && !sqIsAttacked(4) && !sqIsAttacked(4 + EAST) && !sqIsAttacked(4 + EAST*2)) {
				move_add_if_legal(moves, movesInPosition++, 4, 6, KING, EMPTY, MFLAGS_CASTLE_SHORT);
			}
		}
		if ((wCastlingRights & CASTLE_BOTH) || (wCastlingRights & CASTLE_LONG)) {
			if (board[4 + WEST] == EMPTY && board[4 + 2 * WEST] == EMPTY && board[4 + 3 * WEST] == EMPTY && !sqIsAttacked(4) && !sqIsAttacked(4 + WEST) && !sqIsAttacked(4 + 2 * WEST)) {
				move_add_if_legal(moves, movesInPosition++, 4, 2, KING, EMPTY, MFLAGS_CASTLE_LONG);
			}
		}
	}
	else {
		if ((bCastlingRights & CASTLE_BOTH) || (bCastlingRights & CASTLE_SHORT)) {
			if (board[116 + EAST] == EMPTY && board[116 + 2 * EAST] == EMPTY && !sqIsAttacked(116) && !sqIsAttacked(116 + EAST) && !sqIsAttacked(116 + 2 * EAST)) {
				move_add_if_legal(moves, movesInPosition++, 116, 118, KING, EMPTY, MFLAGS_CASTLE_SHORT);
			}
		}
		if ((bCastlingRights & CASTLE_BOTH) || (bCastlingRights & CASTLE_LONG)) {
			if (board[116 + WEST] == EMPTY && board[116 + 2 * WEST] == EMPTY && board[116 + 3 * WEST] == EMPTY && !sqIsAttacked(116) && !sqIsAttacked(116 + WEST) && !sqIsAttacked(116 + 2 * WEST)) {
				move_add_if_legal(moves, movesInPosition++, 116, 114, KING, EMPTY, MFLAGS_CASTLE_LONG);
			}
		}
	}

	for (int sq = 0; sq < 120; sq++) {
		if (ON_BOARD(sq)) {
			if (board[sq] != EMPTY && boardColor[sq] == sideToMove) {  // a players piece on this square
				if (board[sq] <= 2) {  // TODO might have to check for out of bounds here but i dont think it is necessary
					if (board[sq] == PAWN) {
						int dirMod = (sideToMove == WHITE) ? 0 : 4;
						if ((sideToMove == WHITE && sq / 16 == 1) || (sideToMove == BLACK && sq / 16 == 6)) {
							if (board[sq + pieceDeltas[PAWN][dirMod + 3]] == EMPTY) {
								move_add_if_legal(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 3], PAWN, EMPTY, MFLAGS_PAWN_DOUBLE + MFLAGS_PAWN_MOVE);
							}
						}
						if (boardColor[sq + pieceDeltas[PAWN][dirMod]] == (sideToMove*(-1))) {
							move_add_if_legal(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod], PAWN, board[sq + pieceDeltas[PAWN][dirMod]], MFLAGS_CPT + MFLAGS_PAWN_MOVE);
						}
						if (board[sq + pieceDeltas[PAWN][dirMod + 1]] == EMPTY) {
							move_add_if_legal(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 1], PAWN, EMPTY, 0);
						}
						if (boardColor[sq + pieceDeltas[PAWN][dirMod + 2]] == (sideToMove*(-1))) {
							move_add_if_legal(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 2], PAWN, board[sq + pieceDeltas[PAWN][dirMod + 2]], MFLAGS_CPT + MFLAGS_PAWN_MOVE);
						}
						if (enPassant != -1) {
							if ((sq + pieceDeltas[PAWN][dirMod]) == enPassant) {
								move_add_if_legal(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod], PAWN, PAWN, MFLAGS_CPT + MFLAGS_ENP + MFLAGS_PAWN_MOVE);
							}
							else if (sq + pieceDeltas[PAWN][dirMod + 2] == enPassant) {
								move_add_if_legal(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 2], PAWN, PAWN, MFLAGS_CPT + MFLAGS_ENP + MFLAGS_PAWN_MOVE);
							}
						}
					}
					else { // non sliding piece
						for (int dir = 0; dir < 8; dir++) {
							newPos = sq + pieceDeltas[board[sq]][dir];
							if (ON_BOARD(newPos)) {
								if (boardColor[newPos] == sideToMove*(-1)) {
									move_add_if_legal(moves, movesInPosition++, sq, newPos, board[sq], board[newPos], MFLAGS_CPT);
								}
								else if (board[newPos] == EMPTY) {
									move_add_if_legal(moves, movesInPosition++, sq, newPos, board[sq], EMPTY, 0);
								}
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
									move_add_if_legal(moves, movesInPosition++, sq, newPos, board[sq], board[newPos], MFLAGS_CPT);
								}
								break;
							}
							move_add_if_legal(moves, movesInPosition++, sq, newPos, board[sq], EMPTY, 0);
						}
					}
				}
			}
		}
	}

	return movesInPosition;
}

int Board::getLegalMovesInCheck(Move *moves) {
	int numOfMoves = 0;
	int newPos = 0;
	int attackers[8] = { 0 }; // TODO maybe lower this to max two

	int kingSq = (sideToMove == WHITE) ? wKingSq : bKingSq;
	int numOfAttackers = getSqAttackers(attackers, kingSq);

	for (int dir = 0; dir < 8; dir++) {
		newPos = kingSq + pieceDeltas[KING][dir];
		if (ON_BOARD(newPos) && boardColor[newPos] != boardColor[kingSq]) {
			Color colorAtNewPos = boardColor[newPos];
			boardColor[newPos] = sideToMove;
			if (!sqIsAttacked(newPos, kingSq)) {
				if (board[newPos] != EMPTY) {
					move_add_if_legal(moves, numOfMoves++, kingSq, newPos, KING, board[newPos], MFLAGS_CPT);
				}
				else {
					move_add_if_legal(moves, numOfMoves++, kingSq, newPos, KING, EMPTY, 0);
				}
			}
			boardColor[newPos] = colorAtNewPos;
		}
	}

	if (numOfAttackers >= 2) return numOfMoves; // only king moves to non-attacked squares are allowed

	else { // only one attacker
		newPos = kingSq;
		int legalSquares[7] = { 0 };
		int numOfLegalSquares = 0;
		bool canReachLegalSquare;
		if (board[attackers[0]] != KNIGHT) {
			while ((newPos += dirBySquareDiff[kingSq - attackers[0] + 119]) != attackers[0]) {
				legalSquares[numOfLegalSquares++] = newPos;
			}
		}
		legalSquares[numOfLegalSquares++] = attackers[0];

		// TODO can figure out directions there is no reason to search in, e.g. North and South if king is directly south of attacking piece
		// instead of checking if in legalSquares, i can check if it is between kingSq and attackerSq and is a multiple of the direction i.e. ( sq > kingSq and sq <= attackerSq and sq % dir == 0)

		for (int sq = 0; sq < 120; sq++) {
			if (ON_BOARD(sq)) {
				if (boardColor[sq] == sideToMove && sq != kingSq && !sqIsAttacked(kingSq, sq, attackers[0])) {
					if (board[sq] == PAWN) {
						int dirMod = (sideToMove == WHITE) ? 0 : 4;
						if (numOfLegalSquares > 1) {
							if (((sideToMove == WHITE && sq / 16 == 1) || (sideToMove == BLACK && sq / 16 == 6)) && board[(sq + pieceDeltas[PAWN][dirMod + 3]) / 2] == EMPTY) {
								if (std::any_of(std::begin(legalSquares), std::prev(std::end(legalSquares)), [=](int i) {return i == sq + pieceDeltas[PAWN][dirMod + 3]; })) {
									move_add_if_legal(moves, numOfMoves++, sq, sq + pieceDeltas[PAWN][dirMod + 3], PAWN, EMPTY, 0);
								}
								if (std::any_of(std::begin(legalSquares), std::prev(std::end(legalSquares)), [=](int i) {return i == sq + pieceDeltas[PAWN][dirMod + 1]; })) {
									move_add_if_legal(moves, numOfMoves++, sq, sq + pieceDeltas[PAWN][dirMod + 1], PAWN, EMPTY, 0);
								}
							}
						}
						
						if (sq + pieceDeltas[PAWN][dirMod] == attackers[0]) {
							move_add_if_legal(moves, numOfMoves++, sq, sq + pieceDeltas[PAWN][dirMod], PAWN, board[attackers[0]], MFLAGS_CPT + MFLAGS_PAWN_MOVE);
						}
						else if (sq + pieceDeltas[PAWN][dirMod + 2] == attackers[0]) {
							move_add_if_legal(moves, numOfMoves++, sq, sq + pieceDeltas[PAWN][dirMod + 2], PAWN, board[attackers[0]], MFLAGS_CPT + MFLAGS_PAWN_MOVE);
						}
						else if (enPassant != -1 && (board[attackers[0]] == PAWN || std::any_of(std::begin(legalSquares), std::end(legalSquares), [=](int i) {return i == enPassant; }))) {
							if (sq + pieceDeltas[PAWN][dirMod] == enPassant) {
								move_add_if_legal(moves, numOfMoves++, sq, sq + pieceDeltas[PAWN][dirMod], PAWN, board[attackers[0]], MFLAGS_ENP + MFLAGS_CPT + MFLAGS_PAWN_MOVE);
							}
							else if (sq + pieceDeltas[PAWN][dirMod + 2] == enPassant) {
								move_add_if_legal(moves, numOfMoves++, sq, sq + pieceDeltas[PAWN][dirMod + 2], PAWN, board[attackers[0]], MFLAGS_ENP + MFLAGS_CPT + MFLAGS_PAWN_MOVE);
							}
						}
						continue;
					}
					canReachLegalSquare = false;
					for (int i = 0; i < numOfLegalSquares; i++) {
						if (boardColor[sq] == sideToMove && (attackArray[legalSquares[i] - sq + 128] & attackGroups[board[sq]])) {
							canReachLegalSquare = true;
							break;
						}
					}
					if (canReachLegalSquare) {
						if (board[sq] <= 2) {
							for (int dir = 0; dir < 8; dir++) {
								if (std::any_of(std::begin(legalSquares), std::end(legalSquares), [=](int i) {return i == sq + pieceDeltas[board[sq]][dir]; })) {
									if (sq + pieceDeltas[board[sq]][dir] == attackers[0]) {
										move_add_if_legal(moves, numOfMoves++, sq, sq + pieceDeltas[board[sq]][dir], board[sq], board[attackers[0]], MFLAGS_CPT);
									}
									else {
										move_add_if_legal(moves, numOfMoves++, sq, sq + pieceDeltas[board[sq]][dir], board[sq], EMPTY, 0);
									}
								}
							}
						}
						else {
							for (int dir = 0; dir < 8; dir++) {
								newPos = sq;
								while (((newPos += pieceDeltas[board[sq]][dir]) & 0x88) == 0) {
									if (std::any_of(std::begin(legalSquares), std::end(legalSquares), [=](int i) {return i == newPos; })) {
										if (newPos == attackers[0]) {
											move_add_if_legal(moves, numOfMoves++, sq, newPos, board[sq], board[newPos], MFLAGS_CPT);
										}
										else {
											move_add_if_legal(moves, numOfMoves++, sq, newPos, board[sq], EMPTY, 0);
										}
									}
									if (board[newPos] != EMPTY) {
										break;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	return numOfMoves;
}

void Board::move_add_if_legal(Move *moves, int moveNum, int squareFrom, int squareTo, Piece movedPiece, Piece attacked, int flags) {
		moves[moveNum] = Move{ squareFrom, squareTo, movedPiece, attacked, flags };
}

void Board::moveMake(Move move) { // TODO maybe consider writing captured piece here instead of in each move add if that is more efficient
	history[++historyIndex] = State{ move, wCastlingRights, bCastlingRights, enPassant, halfMoveClk };
	if (move.movedPiece == KING) {
		if (boardColor[move.fromSq] == WHITE) {
			wKingSq = move.toSq;
		}
		else {
			bKingSq = move.toSq;
		}
	}
	switch (move.fromSq) {
		case 0: wCastlingRights &= ~CASTLE_LONG; break;
		case 4: wCastlingRights &= ~CASTLE_BOTH; break;
		case 7: wCastlingRights &= ~CASTLE_SHORT; break;
		case 112: bCastlingRights &= ~CASTLE_LONG; break;
		case 116: bCastlingRights &= ~CASTLE_BOTH; break;
		case 119: bCastlingRights &= ~CASTLE_SHORT; break;
	} 
	switch (move.toSq){
		case 0: wCastlingRights &= ~CASTLE_LONG; break;
		case 4: wCastlingRights &= ~CASTLE_BOTH; break;
		case 7: wCastlingRights &= ~CASTLE_SHORT; break;
		case 112: bCastlingRights &= ~CASTLE_LONG; break;
		case 116: bCastlingRights &= ~CASTLE_BOTH; break;
		case 119: bCastlingRights &= ~CASTLE_SHORT; break;
	}

	if (move.flags & MFLAGS_CPT) {
		halfMoveClk = -1;
		materialTotal += (pieceValues[move.attackedPiece] * sideToMove);
	}
	if (move.flags & MFLAGS_PAWN_MOVE) {
		halfMoveClk = -1;
	}

	else if (move.flags & MFLAGS_CASTLE_LONG) {
		board[move.toSq + EAST] = ROOK;
		boardColor[move.toSq + EAST] = boardColor[move.toSq + WEST*2];

		board[move.toSq + WEST*2] = EMPTY;
		boardColor[move.toSq + WEST*2] = NONE;

		if (sideToMove == WHITE) {
			wCastlingRights = 0;
		}
		else {
			bCastlingRights = 0;
		}
	}
	else if (move.flags & MFLAGS_CASTLE_SHORT) {
		board[move.toSq + WEST] = ROOK;
		boardColor[move.toSq + WEST] = boardColor[move.toSq+EAST];

		board[move.toSq + EAST] = EMPTY;
		boardColor[move.toSq + WEST] = NONE;
		if (sideToMove == WHITE) {
			wCastlingRights = 0;
		}
		else {
			bCastlingRights = 0;
		}
	}
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
	halfMoveClk++;
	sideToMove = Color(sideToMove*(-1));
}

void Board::moveUnmake() {
	State prevState = history[historyIndex--];
	sideToMove = Color(sideToMove*(-1));
	if (prevState.move.movedPiece == KING) {
		if (boardColor[prevState.move.toSq] == WHITE) {
			wKingSq = prevState.move.fromSq;
		}
		else {
			bKingSq = prevState.move.fromSq;
		}
	}
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
			board[prevState.move.toSq] = prevState.move.attackedPiece;
			boardColor[prevState.move.toSq] = Color(sideToMove*(-1));
		}
	}
	else {
		board[prevState.move.toSq] = EMPTY;
		boardColor[prevState.move.toSq] = NONE;
	}
	board[prevState.move.fromSq] = prevState.move.movedPiece;
	boardColor[prevState.move.fromSq] = sideToMove;

	wCastlingRights = prevState.wCastleRights;
	bCastlingRights = prevState.bCastleRights;
	enPassant = prevState.enPassant;
	halfMoveClk = prevState.halfMoveClk;
	halfMoveCount--;
}

bool Board::sqIsAttacked(int chkdSq, int xRaySq, int ignoreAttackerOnSq) {
	int lookupVal;
	int newPos;
	Piece xRayPiece;
	Color xRayColor;
	Piece ignoredSq;
	Color ignoredColor;
	bool isAttacked = false;
	if (xRaySq != -1) {
		xRayPiece = board[xRaySq];
		xRayColor = boardColor[xRaySq];
		board[xRaySq] = EMPTY;
		boardColor[xRaySq] = NONE;
	}
	for (int sq = 0; sq < 120; sq++) {
		if (sq != chkdSq && ON_BOARD(sq) && board[sq] != EMPTY && (boardColor[chkdSq] != boardColor[sq]) && sq != ignoreAttackerOnSq) {
			lookupVal = chkdSq - sq + 128;
			if (board[sq] == PAWN) {
				int attackGroup = (boardColor[sq] == WHITE) ? attackGroups[PAWN] - 8 : attackGroups[PAWN] - 4;
				if (attackGroup & attackArray[lookupVal]) {
					isAttacked = true;
					break;
				}
			}
			else {
				if (attackGroups[board[sq]] & attackArray[lookupVal]) { // piece can attack sq
					if (board[sq] <= 2) {
						isAttacked = true;
						break;
					}
					newPos = chkdSq;
					while ((newPos += dirBySquareDiff[chkdSq - sq + 119]) != sq) {
						if (board[newPos] != EMPTY) {
							break;						// TODO maybe use goto for efficiency
						}
					}
					if (newPos == sq) {
						isAttacked = true;
						break;
					}
				}
			}
		}
	}
	if (xRaySq != -1) {
		board[xRaySq] = xRayPiece;
		boardColor[xRaySq] = xRayColor;
	}

	return isAttacked;
}

int Board::getSqAttackers(int *attackingSquares, int chkdSq) {
	int numOfAttackers = 0;
	int lookupVal;
	int newPos;
	for (int sq = 0; sq < 120; sq++) {
		if (sq != chkdSq && ON_BOARD(sq) && (boardColor[chkdSq] == boardColor[sq] * (-1))) {
			lookupVal = chkdSq - sq + 128;
			if (board[sq] == PAWN) {
				int attackGroup = (boardColor[sq] == WHITE) ? attackGroups[PAWN] - 8 : attackGroups[PAWN] - 4;
				if (attackGroup & attackArray[lookupVal]) attackingSquares[numOfAttackers++] = sq;
			}
			else {
				if (attackArray[lookupVal] & attackGroups[board[sq]]) { // piece can attack sq
					if (board[sq] <= 2) {
						attackingSquares[numOfAttackers++] = sq;
						continue;
					}
					newPos = chkdSq;
					while ((newPos += dirBySquareDiff[chkdSq - sq + 119]) != sq) {
						if (board[newPos] != EMPTY) {
							break;						// TODO maybe use goto for efficiency
						}
					}
					if (newPos == sq) attackingSquares[numOfAttackers++] = sq;
				}
			}
		}
	}

	return numOfAttackers;
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
		if (!ON_BOARD(sq)) {
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
		if (!ON_BOARD(sq)) {
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
	fen += (sideToMove == WHITE) ? " w " : " b ";

	bool castling = false;
	if (wCastlingRights & CASTLE_SHORT) {
		fen += "K";
		castling = true;
	}
	if (wCastlingRights & CASTLE_LONG) {
		fen += "Q";
		castling = true;
	}
	if (bCastlingRights & CASTLE_SHORT) {
		fen += "k";
		castling = true;
	}
	if (bCastlingRights & CASTLE_LONG) {
		fen += "q";
		castling = true;
	}
	if (!castling) fen += "- ";

	if (enPassant != -1) {
		fen += char((enPassant % 8) + 97) + std::to_string((enPassant / 16) + 1);
		fen += " ";
	}
	else {
		fen += "- ";
	}

	fen += std::to_string(halfMoveClk);

	fen += " ";

	fen += std::to_string(int(halfMoveCount / 2) + 1);

	return fen;
}