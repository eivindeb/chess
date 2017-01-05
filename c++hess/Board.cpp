#include "stdafx.h"
#include "Board.h"
#include <iostream>
#include <string>
#include <stdlib.h>
#include <algorithm>
#include <sstream>

Board::Board(std::string fen) {
	for (int sq = 0; sq < 120; sq++) {
		if (ON_BOARD(sq)) {
			for (int piece = 0; piece < 6; piece++) {
				zobrist.pieces[piece][0][sq] = rand64();
				zobrist.pieces[piece][1][sq] = rand64();
			}
		}
	}

	for (int i = 0; i < 4; i++) {
		zobrist.wCastlingRights[i] = rand64();
		zobrist.bCastlingRights[i] = rand64();
	}

	for (int i = 0; i < 8; i++) {
		zobrist.enPassant[i] = rand64();
	}

	zobrist.side = rand64();

	if (fen != "false") loadFromFen(fen); 
	else loadFromFen(START_FEN);
}

void Board::loadFromFen(std::string fen) {
	for (int i = 0; i < 18; i++) {
		pieceCount[i] = 0;
	}

	zobristKey = 0;
	phaseFlag = PHASE_MG;
	std::stringstream ss;
	ss.str(fen);
	std::string item;
	std::string fenSplit[6];
	int index = 0;
	while (std::getline(ss, item, ' ')) {
		fenSplit[index++] = item;
	}

	if (fenSplit[1] == "w") {
		sideToMove = WHITE;
	}
	else {
		sideToMove = BLACK;
		zobristKey ^= zobrist.side;
	}
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
	zobristKey ^= zobrist.wCastlingRights[wCastlingRights];
	zobristKey ^= zobrist.bCastlingRights[bCastlingRights];

	if (fenSplit[3] == "-") {
		enPassant = -1;
	}
	else {
		char file = fenSplit[3].at(0);
		int rank = (fenSplit[3].at(1) - '0');
		enPassant = (int(file) - 97) + (rank - 1) * 16;
		zobristKey ^= zobrist.enPassant[enPassant % 8];
	}

	if (fenSplit[4] != "") {
		halfMoveClk = std::stoi(fenSplit[4]);
	}
	else {
		halfMoveClk = 0;
	}

	if (fenSplit[5] != "") {
		halfMoveCount = (std::stoi(fenSplit[5]) * 2);
	}
	else {
		halfMoveCount = 0;
	}
	if (sideToMove == BLACK) halfMoveCount++;

	materialTotal = 0;

	int sq = 112;
	for (char& c : fenSplit[0]) {
		switch (c) {
		case 'P':
			setSq(sq, PAWN, WHITE);
			materialTotal += pieceValues[PAWN];
			break;
		case 'p':
			setSq(sq, PAWN, BLACK);
			materialTotal -= pieceValues[PAWN];
			break;
		case 'N':
			setSq(sq, KNIGHT, WHITE);
			materialTotal += pieceValues[KNIGHT];
			break;
		case 'n':
			setSq(sq, KNIGHT, BLACK);
			materialTotal -= pieceValues[KNIGHT];
			break;
		case 'B':
			setSq(sq, BISHOP, WHITE);
			materialTotal += pieceValues[BISHOP];
			break;
		case 'b':
			setSq(sq, BISHOP, BLACK);
			materialTotal -= pieceValues[BISHOP];
			break;
		case 'R':
			setSq(sq, ROOK, WHITE);
			materialTotal += pieceValues[ROOK];
			break;
		case 'r':
			setSq(sq, ROOK, BLACK);
			materialTotal -= pieceValues[ROOK];
			break;
		case 'Q':
			setSq(sq, QUEEN, WHITE);
			materialTotal += pieceValues[QUEEN];
			break;
		case 'q':
			setSq(sq, QUEEN, BLACK);
			materialTotal -= pieceValues[QUEEN];
			break;
		case 'K':
			setSq(sq, KING, WHITE);
			materialTotal += pieceValues[KING];
			wKingSq = sq;
			break;
		case 'k':
			setSq(sq, KING, BLACK);
			materialTotal -= pieceValues[KING];
			bKingSq = sq;
			break;
		case '/':
			sq -= 25;
			break;
		default: // any number of empty squares
			for (int i = 0; i < int(c - '0'); i++) {
				setSq(sq, EMPTY, NONE);
				sq++;
			}
			sq--;
		}
		sq++;
	}
	int pieceVal = getSideMaterialValue(WHITE);
	if (pieceVal <= 1200) {
		phaseFlag = PHASE_EG;
	}
	pieceVal = getSideMaterialValue(BLACK);
	if (pieceVal <= 1200) {
		phaseFlag = PHASE_EG;
	}
	setPiecePositionTotal();
	historyIndex = -1;
	repIndex = 0;
	repStack[repIndex] = zobristKey;
}

int Board::getSideMaterialValue(Color side) {
	int pieceVal = 0;
	for (int i = 0; i < 6; i++) {
		pieceVal += pieceCount[i + 6*side + 6] * pieceValues[i];
	}

	return pieceVal;
}

int Board::getLegalMoves(Move *moves) {
	int movesInPosition = 0;
	int newPos;

	if (sideToMove == WHITE) {
		if (wCastlingRights & CASTLE_SHORT) {
			if (board[4 + EAST] == EMPTY && board[4 + 2 * EAST] == EMPTY && !sqIsAttacked(4 + EAST, Color(sideToMove*(-1))) && !sqIsAttacked(4 + EAST*2, Color(sideToMove*(-1)))) {
				moveAdd(moves, movesInPosition++, 4, 6, KING, EMPTY, MFLAGS_CASTLE_SHORT);
			}
		}
		if (wCastlingRights & CASTLE_LONG) {
			if (board[4 + WEST] == EMPTY && board[4 + 2 * WEST] == EMPTY && board[4 + 3 * WEST] == EMPTY && !sqIsAttacked(4 + WEST, Color(sideToMove*(-1))) && !sqIsAttacked(4 + 2 * WEST, Color(sideToMove*(-1)))) {
				moveAdd(moves, movesInPosition++, 4, 2, KING, EMPTY, MFLAGS_CASTLE_LONG);
			}
		}
	}
	else {
		if (bCastlingRights & CASTLE_SHORT) {
			if (board[116 + EAST] == EMPTY && board[116 + 2 * EAST] == EMPTY && !sqIsAttacked(116 + EAST, Color(sideToMove*(-1))) && !sqIsAttacked(116 + 2 * EAST, Color(sideToMove*(-1)))) {
				moveAdd(moves, movesInPosition++, 116, 118, KING, EMPTY, MFLAGS_CASTLE_SHORT);
			}
		}
		if (bCastlingRights & CASTLE_LONG) {
			if (board[116 + WEST] == EMPTY && board[116 + 2 * WEST] == EMPTY && board[116 + 3 * WEST] == EMPTY && !sqIsAttacked(116 + WEST, Color(sideToMove*(-1))) && !sqIsAttacked(116 + 2 * WEST, Color(sideToMove*(-1)))) {
				moveAdd(moves, movesInPosition++, 116, 114, KING, EMPTY, MFLAGS_CASTLE_LONG);
			}
		}
	}

	for (int sq = 0; sq < 120; sq++) {
		if (ON_BOARD(sq)) {
			if (board[sq] != EMPTY && boardColor[sq] == sideToMove) {  // a players piece on this square
				if (board[sq] <= 2) {  // TODO might have to check for out of bounds here but i dont think it is necessary (a pawn should never be able to move out of bounds)
					if (board[sq] == PAWN) {
						int dirMod = (sideToMove == WHITE) ? 0 : 4;
						if ((sideToMove == WHITE && sq / 16 == 1) || (sideToMove == BLACK && sq / 16 == 6)) {
							if (board[sq + pieceDeltas[PAWN][dirMod + 3]] == EMPTY && board[sq + pieceDeltas[PAWN][dirMod + 1]] == EMPTY) {
								moveAdd(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 3], PAWN, EMPTY, MFLAGS_PAWN_DOUBLE + MFLAGS_PAWN_MOVE);
							}
						}
						if (ON_BOARD(sq + pieceDeltas[PAWN][dirMod]) && boardColor[sq + pieceDeltas[PAWN][dirMod]] == (sideToMove*(-1))) {
							if ((sideToMove == WHITE && sq / 16 == 6) || (sideToMove == BLACK && sq / 16 == 1)) {
								movesInPosition = addPromotionPermutations(moves, movesInPosition, sq, sq + pieceDeltas[PAWN][dirMod], board[sq + pieceDeltas[PAWN][dirMod]], MFLAGS_CPT);
							}
							else {
								moveAdd(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod], PAWN, board[sq + pieceDeltas[PAWN][dirMod]], MFLAGS_CPT + MFLAGS_PAWN_MOVE);
							}
						}
						if (board[sq + pieceDeltas[PAWN][dirMod + 1]] == EMPTY) {
							if ((sideToMove == WHITE && sq / 16 == 6) || (sideToMove == BLACK && sq / 16 == 1)) {
								movesInPosition = addPromotionPermutations(moves, movesInPosition, sq, sq + pieceDeltas[PAWN][dirMod + 1], board[sq + pieceDeltas[PAWN][dirMod + 1]], 0);
							}
							else {
								moveAdd(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 1], PAWN, board[sq + pieceDeltas[PAWN][dirMod + 1]], MFLAGS_PAWN_MOVE);
							}
						}
						if (ON_BOARD(sq + pieceDeltas[PAWN][dirMod + 2]) && boardColor[sq + pieceDeltas[PAWN][dirMod + 2]] == (sideToMove*(-1))) {
							if ((sideToMove == WHITE && sq / 16 == 6) || (sideToMove == BLACK && sq / 16 == 1)) {
								movesInPosition = addPromotionPermutations(moves, movesInPosition, sq, sq + pieceDeltas[PAWN][dirMod + 2], board[sq + pieceDeltas[PAWN][dirMod + 2]], MFLAGS_CPT);
							}
							else {
								moveAdd(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 2], PAWN, board[sq + pieceDeltas[PAWN][dirMod + 2]], MFLAGS_CPT + MFLAGS_PAWN_MOVE);
							}
						}
						if (enPassant != -1) {
							if ((sq + pieceDeltas[PAWN][dirMod]) == enPassant) {
								moveAdd(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod], PAWN, PAWN, MFLAGS_CPT + MFLAGS_ENP + MFLAGS_PAWN_MOVE);
							}
							else if (sq + pieceDeltas[PAWN][dirMod + 2] == enPassant) {
								moveAdd(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 2], PAWN, PAWN, MFLAGS_CPT + MFLAGS_ENP + MFLAGS_PAWN_MOVE);
							}
						}
					}
					else { // non sliding piece
						for (int dir = 0; dir < 8; dir++) {
							newPos = sq + pieceDeltas[board[sq]][dir];
							if (ON_BOARD(newPos)) {
								if (boardColor[newPos] == sideToMove*(-1)) {
									moveAdd(moves, movesInPosition++, sq, newPos, board[sq], board[newPos], MFLAGS_CPT);
								}
								else if (board[newPos] == EMPTY) {
									moveAdd(moves, movesInPosition++, sq, newPos, board[sq], EMPTY, 0);
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
									moveAdd(moves, movesInPosition++, sq, newPos, board[sq], board[newPos], MFLAGS_CPT);
								}
								break;
							}
							moveAdd(moves, movesInPosition++, sq, newPos, board[sq], EMPTY, 0);
						}
					}
				}
			}
		}
	}

	return movesInPosition;
}

inline int Board::addPromotionPermutations(Move *moves, int moveNum, int sq, int tarSq, Piece attackedPiece, int flags) {
	moveAdd(moves, moveNum++, sq, tarSq, PAWN, attackedPiece, flags + MFLAGS_PROMOTION + MFLAGS_PROMOTION_QUEEN + MFLAGS_PAWN_MOVE);
	moveAdd(moves, moveNum++, sq, tarSq, PAWN, attackedPiece, flags + MFLAGS_PROMOTION + MFLAGS_PROMOTION_ROOK + MFLAGS_PAWN_MOVE);
	moveAdd(moves, moveNum++, sq, tarSq, PAWN, attackedPiece, flags + MFLAGS_PROMOTION + MFLAGS_PROMOTION_BISHOP + MFLAGS_PAWN_MOVE);
	moveAdd(moves, moveNum++, sq, tarSq, PAWN, attackedPiece, flags + MFLAGS_PROMOTION + MFLAGS_PROMOTION_KNIGHT + MFLAGS_PAWN_MOVE);
	return moveNum;
}

int Board::getLegalMovesInCheck(Move *moves) {
	int numOfMoves = 0;
	int newPos = 0;
	int attackers[8] = { 0 }; // TODO maybe lower this to max two

	int kingSq = (sideToMove == WHITE) ? wKingSq : bKingSq;
	int numOfAttackers = getSqAttackers(attackers, kingSq, Color(sideToMove*(-1)));

	for (int dir = 0; dir < 8; dir++) {
		newPos = kingSq + pieceDeltas[KING][dir];
		if (ON_BOARD(newPos) && boardColor[newPos] != boardColor[kingSq]) {
			Color colorAtNewPos = boardColor[newPos];
			boardColor[newPos] = sideToMove;
			if (!sqIsAttacked(newPos, Color(sideToMove*(-1)), kingSq)) {
				if (board[newPos] != EMPTY) {
					moveAdd(moves, numOfMoves++, kingSq, newPos, KING, board[newPos], MFLAGS_CPT);
				}
				else {
					moveAdd(moves, numOfMoves++, kingSq, newPos, KING, EMPTY, 0);
				}
			}
			boardColor[newPos] = colorAtNewPos;
		}
	}

	if (numOfAttackers >= 2) return numOfMoves; // only king moves to non-attacked squares are allowed

	else { // only one attacker
		newPos = kingSq;
		int legalSquares[7] = { -1 };
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
				if (boardColor[sq] == sideToMove && sq != kingSq && !sqIsAttacked(kingSq, Color(sideToMove*(-1)), sq, attackers[0])) {
					if (board[sq] == PAWN) {
						int dirMod = (sideToMove == WHITE) ? 0 : 4;
						if (numOfLegalSquares > 1) {
							if (((sideToMove == WHITE && sq / 16 == 1) || (sideToMove == BLACK && sq / 16 == 6)) && board[(sq + pieceDeltas[PAWN][dirMod + 1])] == EMPTY) {
								if (std::any_of(legalSquares, legalSquares + numOfLegalSquares - 1, [=](int i) {return i == sq + pieceDeltas[PAWN][dirMod + 3]; })) {
									moveAdd(moves, numOfMoves++, sq, sq + pieceDeltas[PAWN][dirMod + 3], PAWN, EMPTY, MFLAGS_PAWN_MOVE + MFLAGS_PAWN_DOUBLE);
								}
							}
							if (std::any_of((legalSquares), legalSquares + numOfLegalSquares - 1, [=](int i) {return i == sq + pieceDeltas[PAWN][dirMod + 1]; })) {
								if ((sideToMove == WHITE && sq / 16 == 6) || (sideToMove == BLACK && sq / 16 == 1)) {
									numOfMoves = addPromotionPermutations(moves, numOfMoves, sq, sq + pieceDeltas[PAWN][dirMod + 1], EMPTY, 0);
								}
								else {
									moveAdd(moves, numOfMoves++, sq, sq + pieceDeltas[PAWN][dirMod + 1], PAWN, EMPTY, MFLAGS_PAWN_MOVE);
								}	
							}
						}
						
						if (sq + pieceDeltas[PAWN][dirMod] == attackers[0]) {
							if ((sideToMove == WHITE && sq / 16 == 6) || (sideToMove == BLACK && sq / 16 == 1)) {
								numOfMoves = addPromotionPermutations(moves, numOfMoves, sq, attackers[0], board[attackers[0]], MFLAGS_CPT);
							}
							else {
								moveAdd(moves, numOfMoves++, sq, attackers[0], PAWN, board[attackers[0]], MFLAGS_CPT + MFLAGS_PAWN_MOVE);
							}
						}
						else if (sq + pieceDeltas[PAWN][dirMod + 2] == attackers[0]) {
							if ((sideToMove == WHITE && sq / 16 == 6) || (sideToMove == BLACK && sq / 16 == 1)) {
								numOfMoves = addPromotionPermutations(moves, numOfMoves, sq, attackers[0], board[attackers[0]], MFLAGS_CPT);
							}
							else {
								moveAdd(moves, numOfMoves++, sq, attackers[0], PAWN, board[attackers[0]], MFLAGS_CPT + MFLAGS_PAWN_MOVE);
							}
						}
						else if (enPassant != -1 && (board[attackers[0]] == PAWN || std::any_of(legalSquares, legalSquares + numOfLegalSquares, [=](int i) {return i == enPassant; }))) {
							if (sq + pieceDeltas[PAWN][dirMod] == enPassant) {
								moveAdd(moves, numOfMoves++, sq, sq + pieceDeltas[PAWN][dirMod], PAWN, board[attackers[0]], MFLAGS_ENP + MFLAGS_CPT + MFLAGS_PAWN_MOVE);
							}
							else if (sq + pieceDeltas[PAWN][dirMod + 2] == enPassant) {
								moveAdd(moves, numOfMoves++, sq, sq + pieceDeltas[PAWN][dirMod + 2], PAWN, board[attackers[0]], MFLAGS_ENP + MFLAGS_CPT + MFLAGS_PAWN_MOVE);
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
								if (std::any_of(legalSquares, legalSquares + numOfLegalSquares, [=](int i) {return i == sq + pieceDeltas[board[sq]][dir]; })) {
									if (sq + pieceDeltas[board[sq]][dir] == attackers[0]) {
										moveAdd(moves, numOfMoves++, sq, sq + pieceDeltas[board[sq]][dir], board[sq], board[attackers[0]], MFLAGS_CPT);
									}
									else {
										moveAdd(moves, numOfMoves++, sq, sq + pieceDeltas[board[sq]][dir], board[sq], EMPTY, 0);
									}
								}
							}
						}
						else {
							for (int dir = 0; dir < 8; dir++) {
								newPos = sq;
								while (((newPos += pieceDeltas[board[sq]][dir]) & 0x88) == 0) {
									if (std::any_of(legalSquares, legalSquares + numOfLegalSquares, [=](int i) {return i == newPos; })) {
										if (newPos == attackers[0]) {
											moveAdd(moves, numOfMoves++, sq, newPos, board[sq], board[newPos], MFLAGS_CPT);
										}
										else {
											moveAdd(moves, numOfMoves++, sq, newPos, board[sq], EMPTY, 0);
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

int Board::getCaptureMoves(Move *moves) {
	int movesInPosition = 0;
	int newPos;

	for (int sq = 0; sq < 120; sq++) {
		if (ON_BOARD(sq)) {
			if (board[sq] != EMPTY && boardColor[sq] == sideToMove) {  // a players piece on this square
				if (board[sq] <= 2) {
					if (board[sq] == PAWN) {
						int dirMod = (sideToMove == WHITE) ? 0 : 4;
						if (ON_BOARD(sq + pieceDeltas[PAWN][dirMod]) && boardColor[sq + pieceDeltas[PAWN][dirMod]] == (sideToMove*(-1))) {
							if ((sideToMove == WHITE && sq / 16 == 6) || (sideToMove == BLACK && sq / 16 == 1)) {
								movesInPosition = addPromotionPermutations(moves, movesInPosition, sq, sq + pieceDeltas[PAWN][dirMod], board[sq + pieceDeltas[PAWN][dirMod]], MFLAGS_CPT);
							}
							else {
								moveAdd(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod], PAWN, board[sq + pieceDeltas[PAWN][dirMod]], MFLAGS_CPT + MFLAGS_PAWN_MOVE);
							}
						}
						if (ON_BOARD(sq + pieceDeltas[PAWN][dirMod + 2]) && boardColor[sq + pieceDeltas[PAWN][dirMod + 2]] == (sideToMove*(-1))) {
							if ((sideToMove == WHITE && sq / 16 == 6) || (sideToMove == BLACK && sq / 16 == 1)) {
								movesInPosition = addPromotionPermutations(moves, movesInPosition, sq, sq + pieceDeltas[PAWN][dirMod + 2], board[sq + pieceDeltas[PAWN][dirMod + 2]], MFLAGS_CPT);
							}
							else {
								moveAdd(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 2], PAWN, board[sq + pieceDeltas[PAWN][dirMod + 2]], MFLAGS_CPT + MFLAGS_PAWN_MOVE);
							}
						}
						if (enPassant != -1) {
							if ((sq + pieceDeltas[PAWN][dirMod]) == enPassant) {
								moveAdd(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod], PAWN, PAWN, MFLAGS_CPT + MFLAGS_ENP + MFLAGS_PAWN_MOVE);
							}
							else if (sq + pieceDeltas[PAWN][dirMod + 2] == enPassant) {
								moveAdd(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 2], PAWN, PAWN, MFLAGS_CPT + MFLAGS_ENP + MFLAGS_PAWN_MOVE);
							}
						}
					}
					else { // non sliding piece
						for (int dir = 0; dir < 8; dir++) {
							newPos = sq + pieceDeltas[board[sq]][dir];
							if (ON_BOARD(newPos)) {
								if (boardColor[newPos] == sideToMove*(-1)) {
									moveAdd(moves, movesInPosition++, sq, newPos, board[sq], board[newPos], MFLAGS_CPT);
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
									moveAdd(moves, movesInPosition++, sq, newPos, board[sq], board[newPos], MFLAGS_CPT);
								}
								break;
							}
						}
					}
				}
			}
		}
	}

	return movesInPosition;
}

inline void Board::moveAdd(Move *moves, int moveNum, int squareFrom, int squareTo, Piece movedPiece, Piece attacked, int flags) {
	moves[moveNum] = Move{ static_cast<uint8_t>(squareFrom), static_cast<uint8_t>(squareTo), movedPiece, attacked, static_cast<uint16_t>(flags), static_cast<uint8_t>(moveNum) };
}

void Board::moveMake(Move move) { // TODO maybe consider writing captured piece here instead of in each move add if that is more efficient
	history[++historyIndex] = State{ move, wCastlingRights, bCastlingRights, enPassant, halfMoveClk };
	repStack[++repIndex] = zobristKey;
	if (move.movedPiece == KING) {
		if (boardColor[move.fromSq] == WHITE) {
			wKingSq = move.toSq;
		}
		else {
			bKingSq = move.toSq;
		}
	}
	zobristKey ^= zobrist.wCastlingRights[wCastlingRights];
	zobristKey ^= zobrist.bCastlingRights[bCastlingRights];
	if (enPassant != -1) zobristKey ^= zobrist.enPassant[enPassant % 8];

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
		clearSq(move.toSq);
	}
	else if (move.flags & MFLAGS_PAWN_MOVE) { // possible bug source, have else if since at the moment they do the same
		halfMoveClk = -1;
	}

	if (move.flags & MFLAGS_PROMOTION) {
		if (move.flags & MFLAGS_PROMOTION_QUEEN) {
			materialTotal += (pieceValues[QUEEN] - pieceValues[PAWN])*sideToMove;
			clearSq(move.fromSq);
			setSq(move.fromSq, QUEEN, sideToMove);
		}
		else if (move.flags & MFLAGS_PROMOTION_ROOK) {
			materialTotal += (pieceValues[ROOK] - pieceValues[PAWN])*sideToMove;
			clearSq(move.fromSq);
			setSq(move.fromSq, ROOK, sideToMove);
		}
		else if (move.flags & MFLAGS_PROMOTION_BISHOP) {
			materialTotal += (pieceValues[BISHOP] - pieceValues[PAWN])*sideToMove;
			clearSq(move.fromSq);
			setSq(move.fromSq, BISHOP, sideToMove);
		}
		else if (move.flags & MFLAGS_PROMOTION_KNIGHT) {
			materialTotal += (pieceValues[KNIGHT] - pieceValues[PAWN])*sideToMove;
			clearSq(move.fromSq);
			setSq(move.fromSq, KNIGHT, sideToMove);
		}
	}

	else if (move.flags & MFLAGS_CASTLE_LONG) {
		setSq(move.toSq + EAST, ROOK, sideToMove);
		clearSq(move.toSq + WEST * 2);

		if (sideToMove == WHITE) {
			wCastlingRights = 0;
		}
		else {
			bCastlingRights = 0;
		}
	}
	else if (move.flags & MFLAGS_CASTLE_SHORT) {
		setSq(move.toSq + WEST, ROOK, sideToMove);
		clearSq(move.toSq + EAST);

		if (sideToMove == WHITE) {
			wCastlingRights = 0;
		}
		else {
			bCastlingRights = 0;
		}
	}

	setSq(move.toSq, board[move.fromSq], boardColor[move.fromSq]);
	clearSq(move.fromSq);

	if (move.flags & MFLAGS_ENP) {
		if ((move.toSq % 8) - (move.fromSq % 8) == WEST) {
			clearSq(move.fromSq + WEST);
		}
		else {
			clearSq(move.fromSq + EAST);
		}
	}

	if (move.flags & MFLAGS_PAWN_DOUBLE) {
		enPassant = (move.fromSq + move.toSq) / 2;
		zobristKey ^= zobrist.enPassant[enPassant % 8];
	} else if (enPassant != -1) {
		enPassant = -1;
	}

	halfMoveCount++;
	halfMoveClk++;
	sideToMove = Color(sideToMove*(-1));
	zobristKey ^= zobrist.side;
	zobristKey ^= zobrist.wCastlingRights[wCastlingRights];
	zobristKey ^= zobrist.bCastlingRights[bCastlingRights];
	//if (zobristKey != getZobristKey()) {
	//	move.flags = 0;
	//}
}

unsigned long long Board::getZobristKey() {
	unsigned long long key = 0;
	if (sideToMove == BLACK) {
		key ^= zobrist.side;
	}
	for (int sq = 0; sq < 120; sq++) {
		if (ON_BOARD(sq) && board[sq] != EMPTY) {
			int color = (boardColor[sq] == WHITE) ? 1 : 0;
			key ^= zobrist.pieces[board[sq]][color][sq];
		}
	}
	key ^= zobrist.wCastlingRights[wCastlingRights];
	key ^= zobrist.bCastlingRights[bCastlingRights];
	if (enPassant != -1) {
		key ^= zobrist.enPassant[enPassant % 8];
	}
	
	return key;
}

void Board::moveUnmake() {
	State prevState = history[historyIndex--];
	sideToMove = Color(sideToMove*(-1));
	zobristKey ^= zobrist.side;
	if (enPassant != -1) zobristKey ^= zobrist.enPassant[enPassant % 8];
	zobristKey ^= zobrist.wCastlingRights[wCastlingRights];
	zobristKey ^= zobrist.bCastlingRights[bCastlingRights];

	if (prevState.move.movedPiece == KING) {
		if (boardColor[prevState.move.toSq] == WHITE) {
			wKingSq = prevState.move.fromSq;
		}
		else {
			bKingSq = prevState.move.fromSq;
		}
		if (prevState.move.flags & MFLAGS_CASTLE_LONG) {
			setSq(prevState.move.toSq + 2 * WEST, ROOK, sideToMove);
			clearSq(prevState.move.toSq + EAST);
		}
		else if (prevState.move.flags & MFLAGS_CASTLE_SHORT) {
			setSq(prevState.move.toSq + EAST, ROOK, sideToMove);
			clearSq(prevState.move.toSq + WEST);
		}
	}
	if (prevState.move.flags & MFLAGS_PROMOTION) {
		if (prevState.move.flags & MFLAGS_PROMOTION_QUEEN) {
			materialTotal -= (pieceValues[QUEEN] - pieceValues[PAWN])*sideToMove;
		}
		else if (prevState.move.flags & MFLAGS_PROMOTION_ROOK) {
			materialTotal -= (pieceValues[ROOK] - pieceValues[PAWN])*sideToMove;
		}
		else if (prevState.move.flags & MFLAGS_PROMOTION_BISHOP) {
			materialTotal -= (pieceValues[BISHOP] - pieceValues[PAWN])*sideToMove;
		}
		else if (prevState.move.flags & MFLAGS_PROMOTION_KNIGHT) {
			materialTotal -= (pieceValues[KNIGHT] - pieceValues[PAWN])*sideToMove;
		}
	}
	clearSq(prevState.move.toSq); // must clear first to remove position value of piece
	if (prevState.move.flags & MFLAGS_CPT) {
		materialTotal -= (pieceValues[prevState.move.attackedPiece] * sideToMove);
		if (prevState.move.flags & MFLAGS_ENP) {
			if ((prevState.move.toSq % 8) - (prevState.move.fromSq % 8) == WEST) {
				setSq(prevState.move.fromSq + WEST, PAWN, Color(sideToMove*(-1)));
			}
			else {
				setSq(prevState.move.fromSq + EAST, PAWN, Color(sideToMove*(-1)));
			}
			clearSq(prevState.move.toSq);
		}
		else {
			setSq(prevState.move.toSq, prevState.move.attackedPiece, Color(sideToMove*(-1)));
		}
	}

	setSq(prevState.move.fromSq, prevState.move.movedPiece, sideToMove);

	wCastlingRights = prevState.wCastleRights;
	bCastlingRights = prevState.bCastleRights;
	enPassant = prevState.enPassant;
	halfMoveClk = prevState.halfMoveClk;
	halfMoveCount--;
	if (enPassant != -1) zobristKey ^= zobrist.enPassant[enPassant % 8];
	zobristKey ^= zobrist.wCastlingRights[wCastlingRights];
	zobristKey ^= zobrist.bCastlingRights[bCastlingRights];

	--repIndex;
	//if (zobristKey != getZobristKey()) {
	//	prevState.move.flags = 0;
	//}
}

int Board::calculatePositionTotal() {
	int posTotal = 0;
	int *tablePtr;
	for (int sq = 0; sq < 120; sq++) {
		if (ON_BOARD(sq)) {
			if (board[sq] != EMPTY) {
				switch (board[sq]) {
				case PAWN:
					if (phaseFlag & PHASE_MG) {
						tablePtr = (boardColor[sq] == WHITE) ? PSTwPawnMG : PSTbPawnMG;
					}
					else if (phaseFlag & PHASE_EG) {
						tablePtr = (boardColor[sq] == WHITE) ? PSTwPawnEG : PSTbPawnEG;
					}
					else {
						tablePtr = PSTwPawnMG;
						throw "Unknown position flag";
					}
					break;
				case KNIGHT:
					if (phaseFlag & PHASE_MG) {
						tablePtr = (boardColor[sq] == WHITE) ? PSTwKnightMG : PSTbKnightMG;
					}
					else if (phaseFlag & PHASE_EG) {
						tablePtr = (boardColor[sq] == WHITE) ? PSTwKnightEG : PSTbKnightEG;
					}
					else {
						tablePtr = PSTwPawnMG;
						throw "Unknown position flag";
					}
					break;
				case BISHOP:
					if (phaseFlag & PHASE_MG) {
						tablePtr = (boardColor[sq] == WHITE) ? PSTwBishopMG : PSTbBishopMG;
					}
					else if (phaseFlag & PHASE_EG) {
						tablePtr = (boardColor[sq] == WHITE) ? PSTwBishopEG : PSTbBishopEG;
					}
					else {
						tablePtr = PSTwPawnMG;
						throw "Unknown position flag";
					}
					break;
				case ROOK:
					if (phaseFlag & PHASE_MG) {
						tablePtr = (boardColor[sq] == WHITE) ? PSTwRookMG : PSTbRookMG;
					}
					else if (phaseFlag & PHASE_EG) {
						tablePtr = (boardColor[sq] == WHITE) ? PSTwRookEG : PSTbRookEG;
					}
					else {
						tablePtr = PSTwPawnMG;
						throw "Unknown position flag";
					}
					break;
				case QUEEN:
					if (phaseFlag & PHASE_MG) {
						tablePtr = (boardColor[sq] == WHITE) ? PSTwQueenMG : PSTbQueenMG;
					}
					else if (phaseFlag & PHASE_EG) {
						tablePtr = (boardColor[sq] == WHITE) ? PSTwQueenEG : PSTbQueenEG;
					}
					else {
						tablePtr = PSTwPawnMG;
						throw "Unknown position flag";
					}
					break;
				case KING:
					if (phaseFlag & PHASE_MG) {
						tablePtr = (boardColor[sq] == WHITE) ? PSTwKingMG : PSTbKingMG;
					}
					else if (phaseFlag & PHASE_EG) {
						tablePtr = (boardColor[sq] == WHITE) ? PSTwKingEG : PSTbKingEG;
					}
					else {
						tablePtr = PSTwPawnMG;
						throw "Unknown position flag";
					}
					break;
				default:
					tablePtr = PSTwPawnMG;
					throw "Unkown piece type";
				}
				posTotal += (tablePtr[sq] * boardColor[sq]);
			}
		}
	}

	return posTotal;
}

inline void Board::clearSq(int sq) {
	if (board[sq] != EMPTY) {
		int *tablePtr;
		switch (board[sq]) {
		case PAWN:
			if (phaseFlag & PHASE_MG) {
				tablePtr = (boardColor[sq] == WHITE) ? PSTwPawnMG : PSTbPawnMG;
			}
			else if (phaseFlag & PHASE_EG) {
				tablePtr = (boardColor[sq] == WHITE) ? PSTwPawnEG : PSTbPawnEG;
			}
			else {
				tablePtr = PSTwPawnMG;
				throw "Unknown position flag";
			}
			break;
		case KNIGHT:
			if (phaseFlag & PHASE_MG) {
				tablePtr = (boardColor[sq] == WHITE) ? PSTwKnightMG : PSTbKnightMG;
			}
			else if (phaseFlag & PHASE_EG) {
				tablePtr = (boardColor[sq] == WHITE) ? PSTwKnightEG : PSTbKnightEG;
			}
			else {
				tablePtr = PSTwPawnMG;
				throw "Unknown position flag";
			}
			break;
		case BISHOP:
			if (phaseFlag & PHASE_MG) {
				tablePtr = (boardColor[sq] == WHITE) ? PSTwBishopMG : PSTbBishopMG;
			}
			else if (phaseFlag & PHASE_EG) {
				tablePtr = (boardColor[sq] == WHITE) ? PSTwBishopEG : PSTbBishopEG;
			}
			else {
				tablePtr = PSTwPawnMG;
				throw "Unknown position flag";
			}
			break;
		case ROOK:
			if (phaseFlag & PHASE_MG) {
				tablePtr = (boardColor[sq] == WHITE) ? PSTwRookMG : PSTbRookMG;
			}
			else if (phaseFlag & PHASE_EG) {
				tablePtr = (boardColor[sq] == WHITE) ? PSTwRookEG : PSTbRookEG;
			}
			else {
				tablePtr = PSTwPawnMG;
				throw "Unknown position flag";
			}
			break;
		case QUEEN:
			if (phaseFlag & PHASE_MG) {
				tablePtr = (boardColor[sq] == WHITE) ? PSTwQueenMG : PSTbQueenMG;
			}
			else if (phaseFlag & PHASE_EG) {
				tablePtr = (boardColor[sq] == WHITE) ? PSTwQueenEG : PSTbQueenEG;
			}
			else {
				tablePtr = PSTwPawnMG;
				throw "Unknown position flag";
			}
			break;
		case KING:
			if (phaseFlag & PHASE_MG) {
				tablePtr = (boardColor[sq] == WHITE) ? PSTwKingMG : PSTbKingMG;
			}
			else if (phaseFlag & PHASE_EG) {
				tablePtr = (boardColor[sq] == WHITE) ? PSTwKingEG : PSTbKingEG;
			}
			else {
				tablePtr = PSTwPawnMG;
				throw "Unknown position flag";
			}
			break;
		default:
			tablePtr = PSTwPawnEG;
			throw "Unknown piece type";
		}
		pieceCount[board[sq] + 6 * boardColor[sq] + 6]--;
		positionTotal -= boardColor[sq] * tablePtr[sq];
		int side = boardColor[sq] == WHITE ? 1 : 0;
		zobristKey ^= zobrist.pieces[board[sq]][side][sq];
	}
	board[sq] = EMPTY;
	boardColor[sq] = NONE;
}

inline void Board::setSq(int sq, Piece piece, Color side) {
	if (piece != EMPTY) {
		int *tablePtr;
		switch (piece) {
			case PAWN:
				if (phaseFlag & PHASE_MG) {
					tablePtr = (side == WHITE) ? PSTwPawnMG : PSTbPawnMG;
				}
				else if (phaseFlag & PHASE_EG) {
					tablePtr = (side == WHITE) ? PSTwPawnEG : PSTbPawnEG;
				}
				else {
					tablePtr = PSTwPawnMG;
					throw "Unknown position flag";
				}
				break;
			case KNIGHT:
				if (phaseFlag & PHASE_MG) {
					tablePtr = (side == WHITE) ? PSTwKnightMG : PSTbKnightMG;
				}
				else if (phaseFlag & PHASE_EG) {
					tablePtr = (side == WHITE) ? PSTwKnightEG : PSTbKnightEG;
				}
				else {
					tablePtr = PSTwPawnMG;
					throw "Unknown position flag";
				}
				break;
			case BISHOP:
				if (phaseFlag & PHASE_MG) {
					tablePtr = (side == WHITE) ? PSTwBishopMG : PSTbBishopMG;
				}
				else if (phaseFlag & PHASE_EG) {
					tablePtr = (side == WHITE) ? PSTwBishopEG : PSTbBishopEG;
				}
				else {
					tablePtr = PSTwPawnMG;
					throw "Unknown position flag";
				}
				break;
			case ROOK:
				if (phaseFlag & PHASE_MG) {
					tablePtr = (side == WHITE) ? PSTwRookMG : PSTbRookMG;
				}
				else if (phaseFlag & PHASE_EG) {
					tablePtr = (side == WHITE) ? PSTwRookEG : PSTbRookEG;
				}
				else {
					tablePtr = PSTwPawnMG;
					throw "Unknown position flag";
				}
				break;
			case QUEEN:
				if (phaseFlag & PHASE_MG) {
					tablePtr = (side == WHITE) ? PSTwQueenMG : PSTbQueenMG;
				}
				else if (phaseFlag & PHASE_EG) {
					tablePtr = (side == WHITE) ? PSTwQueenEG : PSTbQueenEG;
				}
				else {
					tablePtr = PSTwPawnMG;
					throw "Unknown position flag";
				}
				break;
			case KING:
				if (phaseFlag & PHASE_MG) {
					tablePtr = (side == WHITE) ? PSTwKingMG : PSTbKingMG;
				}
				else if (phaseFlag & PHASE_EG) {
					tablePtr = (side == WHITE) ? PSTwKingEG : PSTbKingEG;
				}
				else {
					tablePtr = PSTwPawnMG;
					throw "Unknown position flag";
				}
				break;
			default:
				tablePtr = PSTwPawnEG;
				throw "Unknown piece type";
			}
		pieceCount[piece + 6 * side + 6]++;
		positionTotal += side * tablePtr[sq];
		int sqSide = side == WHITE ? 1 : 0;
		zobristKey ^= zobrist.pieces[piece][sqSide][sq];
	}
	board[sq] = piece;
	boardColor[sq] = side;
}

inline void Board::setPiecePositionTotal() {
	positionTotal = 0;
	int *tablePtr;
	for (int sq = 0; sq < 120; sq++) {
		if (ON_BOARD(sq)) {
			if (board[sq] != EMPTY) {
				switch (board[sq]) {
					case PAWN:
						if (phaseFlag & PHASE_MG) {
							tablePtr = (boardColor[sq] == WHITE) ? PSTwPawnMG : PSTbPawnMG;
						}
						else if (phaseFlag & PHASE_EG) {
							tablePtr = (boardColor[sq] == WHITE) ? PSTwPawnEG : PSTbPawnEG;
						}
						else {
							tablePtr = PSTwPawnMG;
							throw "Unknown position flag";
						}
						break;
					case KNIGHT:
						if (phaseFlag & PHASE_MG) {
							tablePtr = (boardColor[sq] == WHITE) ? PSTwKnightMG : PSTbKnightMG;
						}
						else if (phaseFlag & PHASE_EG) {
							tablePtr = (boardColor[sq] == WHITE) ? PSTwKnightEG : PSTbKnightEG;
						}
						else {
							tablePtr = PSTwPawnMG;
							throw "Unknown position flag";
						}
						break;
					case BISHOP:
						if (phaseFlag & PHASE_MG) {
							tablePtr = (boardColor[sq] == WHITE) ? PSTwBishopMG : PSTbBishopMG;
						}
						else if (phaseFlag & PHASE_EG) {
							tablePtr = (boardColor[sq] == WHITE) ? PSTwBishopEG : PSTbBishopEG;
						}
						else {
							tablePtr = PSTwPawnMG;
							throw "Unknown position flag";
						}
						break;
					case ROOK:
						if (phaseFlag & PHASE_MG) {
							tablePtr = (boardColor[sq] == WHITE) ? PSTwRookMG : PSTbRookMG;
						}
						else if (phaseFlag & PHASE_EG) {
							tablePtr = (boardColor[sq] == WHITE) ? PSTwRookEG : PSTbRookEG;
						}
						else {
							tablePtr = PSTwPawnMG;
							throw "Unknown position flag";
						}
						break;
					case QUEEN:
						if (phaseFlag & PHASE_MG) {
							tablePtr = (boardColor[sq] == WHITE) ? PSTwQueenMG : PSTbQueenMG;
						}
						else if (phaseFlag & PHASE_EG) {
							tablePtr = (boardColor[sq] == WHITE) ? PSTwQueenEG : PSTbQueenEG;
						}
						else {
							tablePtr = PSTwPawnMG;
							throw "Unknown position flag";
						}
						break;
					case KING:
						if (phaseFlag & PHASE_MG) {
							tablePtr = (boardColor[sq] == WHITE) ? PSTwKingMG : PSTbKingMG;
						}
						else if (phaseFlag & PHASE_EG) {
							tablePtr = (boardColor[sq] == WHITE) ? PSTwKingEG : PSTbKingEG;
						}
						else {
							tablePtr = PSTwPawnMG;
							throw "Unknown position flag";
						}
						break;
					default:
						tablePtr = PSTwPawnMG;
						throw "Unkown piece type";
				}
				positionTotal += (tablePtr[sq] * boardColor[sq]);
			}
		}
	}
}

bool Board::inCheck(Color side) {
	int kingSq = (side == WHITE) ? wKingSq : bKingSq;
	if (sqIsAttacked(kingSq, Color(side*(-1)))) return true;
	return false;
}

bool Board::sqIsAttacked(int chkdSq, Color attackColor, int xRaySq, int ignoreAttackerOnSq) {
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
		if (ON_BOARD(sq) && sq != chkdSq && board[sq] != EMPTY && (boardColor[sq] == attackColor) && sq != ignoreAttackerOnSq) {
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

int Board::getSqAttackers(int *attackingSquares, int chkdSq, Color attackColor) {
	int numOfAttackers = 0;
	int lookupVal;
	int newPos;
	for (int sq = 0; sq < 120; sq++) {
		if (sq != chkdSq && ON_BOARD(sq) && (boardColor[sq] == attackColor)) {
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
		std::cout << " id: " << int(moves[i].id);
		if (moves[i].flags & MFLAGS_CPT) {
			std::cout << "\tCapture";
		} if (moves[i].flags & MFLAGS_CASTLE_LONG) {
			std::cout << "\tCastle long";
		} if (moves[i].flags & MFLAGS_CASTLE_SHORT) {
			std::cout << "\tCastle short";
		} if (moves[i].flags & MFLAGS_ENP) {
			std::cout << "\tEn Passant";
		} if (moves[i].flags & MFLAGS_PROMOTION) {
			std::cout << "\tPromotion";
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

/* function taken from Sungorus chess engine */
inline unsigned long long Board::rand64() {
	static unsigned long long next = 1;

	next = next * 1103515245 + 12345;
	return next;
}