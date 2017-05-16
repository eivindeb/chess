#include "stdafx.h"
#include "Board.h"
#include <iostream>
#include <string>
#include <stdlib.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

Board::Board(std::string fen) {
	//generate random zobrist seeds
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

	// load optional parameter or base FEN (board position)
	if (fen != "false") loadFromFen(fen); 
	else loadFromFen(START_FEN);
}

void Board::loadFromFen(std::string fen) {
	//reset piece lists (count and position)
	for (int i = 0; i < 12; i++) {
		pieceLists[i][COUNT] = 0;
		for (int j = 1; j < 11; j++) {
			pieceLists[i][j] = -1;
		}
	}

	//set variables according to FEN

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
			pieceListAddPiece(sq, PAWN, WHITE);
			break;
		case 'p':
			setSq(sq, PAWN, BLACK);
			materialTotal -= pieceValues[PAWN];
			pieceListAddPiece(sq, PAWN, BLACK);
			break;
		case 'N':
			setSq(sq, KNIGHT, WHITE);
			materialTotal += pieceValues[KNIGHT];
			pieceListAddPiece(sq, KNIGHT, WHITE);
			break;
		case 'n':
			setSq(sq, KNIGHT, BLACK);
			materialTotal -= pieceValues[KNIGHT];
			pieceListAddPiece(sq, KNIGHT, BLACK);
			break;
		case 'B':
			setSq(sq, BISHOP, WHITE);
			materialTotal += pieceValues[BISHOP];
			pieceListAddPiece(sq, BISHOP, WHITE);
			break;
		case 'b':
			setSq(sq, BISHOP, BLACK);
			materialTotal -= pieceValues[BISHOP];
			pieceListAddPiece(sq, BISHOP, BLACK);
			break;
		case 'R':
			setSq(sq, ROOK, WHITE);
			materialTotal += pieceValues[ROOK];
			pieceListAddPiece(sq, ROOK, WHITE);
			break;
		case 'r':
			setSq(sq, ROOK, BLACK);
			materialTotal -= pieceValues[ROOK];
			pieceListAddPiece(sq, ROOK, BLACK);
			break;
		case 'Q':
			setSq(sq, QUEEN, WHITE);
			materialTotal += pieceValues[QUEEN];
			pieceListAddPiece(sq, QUEEN, WHITE);
			break;
		case 'q':
			setSq(sq, QUEEN, BLACK);
			materialTotal -= pieceValues[QUEEN];
			pieceListAddPiece(sq, QUEEN, BLACK);
			break;
		case 'K':
			setSq(sq, KING, WHITE);
			materialTotal += pieceValues[KING];
			pieceListAddPiece(sq, KING, WHITE);
			wKingSq = sq;
			break;
		case 'k':
			setSq(sq, KING, BLACK);
			materialTotal -= pieceValues[KING];
			pieceListAddPiece(sq, KING, BLACK);
			bKingSq = sq;
			break;
		case '/':
			sq -= 25;
			break;
		default: // any number of empty squares
			for (int i = 0; i < int(c - '0'); i++) {
				setSq(sq, EMPTY, NONE);
				boardPieceIndex[sq] = -1;
				sq++;
			}
			sq--;
		}
		sq++;
	}

	// set phase flag depending on material values
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
		pieceVal += pieceLists[(side == WHITE) ? i + 6 : i][0] * pieceValues[i];
	}

	return pieceVal;
}

int Board::getLegalMoves(int *moves) {
	int movesInPosition = 0;
	int newPos;

	// first check for castling moves
	if (sideToMove == WHITE) {
		if (wCastlingRights & CASTLE_SHORT) {
			if (board[4 + EAST] == EMPTY && board[4 + 2 * EAST] == EMPTY && !sqIsAttacked(4 + EAST, Color(sideToMove*(-1))) && !sqIsAttacked(4 + EAST*2, Color(sideToMove*(-1)))) {
				moveAdd(moves, movesInPosition, 4, 6, KING, EMPTY, 0);
				moves[movesInPosition++] |= (MOVE_CASTLE_SHORT << MOVE_CASTLE_LONG_SHIFT);
			}
		}
		if (wCastlingRights & CASTLE_LONG) {
			if (board[4 + WEST] == EMPTY && board[4 + 2 * WEST] == EMPTY && board[4 + 3 * WEST] == EMPTY && !sqIsAttacked(4 + WEST, Color(sideToMove*(-1))) && !sqIsAttacked(4 + 2 * WEST, Color(sideToMove*(-1)))) {
				moveAdd(moves, movesInPosition, 4, 2, KING, EMPTY, 0);
				moves[movesInPosition++] |= (MOVE_CASTLE_LONG << MOVE_CASTLE_LONG_SHIFT);
			}
		}
	}
	else {
		if (bCastlingRights & CASTLE_SHORT) {
			if (board[116 + EAST] == EMPTY && board[116 + 2 * EAST] == EMPTY && !sqIsAttacked(116 + EAST, Color(sideToMove*(-1))) && !sqIsAttacked(116 + 2 * EAST, Color(sideToMove*(-1)))) {
				moveAdd(moves, movesInPosition, 116, 118, KING, EMPTY, 0);
				moves[movesInPosition++] |= (MOVE_CASTLE_SHORT << MOVE_CASTLE_LONG_SHIFT);
			}
		}
		if (bCastlingRights & CASTLE_LONG) {
			if (board[116 + WEST] == EMPTY && board[116 + 2 * WEST] == EMPTY && board[116 + 3 * WEST] == EMPTY && !sqIsAttacked(116 + WEST, Color(sideToMove*(-1))) && !sqIsAttacked(116 + 2 * WEST, Color(sideToMove*(-1)))) {
				moveAdd(moves, movesInPosition, 116, 114, KING, EMPTY, 0);
				moves[movesInPosition++] |= (MOVE_CASTLE_LONG << MOVE_CASTLE_LONG_SHIFT);
			}
		}
	}

	for (int pieceType = 0; pieceType < 6; pieceType++) {
		for (int pieceIndex = 1; pieceIndex <= pieceLists[(sideToMove == WHITE) ? 6 + pieceType : pieceType][COUNT]; pieceIndex++) {
			int sq = pieceLists[(sideToMove == WHITE) ? 6 + pieceType : pieceType][pieceIndex];
			if (pieceType <= 2) {
				if (pieceType == PAWN) {
					int dirMod = (sideToMove == WHITE) ? 0 : 4;
					if ((sideToMove == WHITE && (sq >> 4) == 1) || (sideToMove == BLACK && (sq >> 4) == 6)) {
						if (board[sq + pieceDeltas[PAWN][dirMod + 3]] == EMPTY && board[sq + pieceDeltas[PAWN][dirMod + 1]] == EMPTY) {
							moveAdd(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 3], PAWN, EMPTY, 0);
						}
					}
					if (ON_BOARD(sq + pieceDeltas[PAWN][dirMod]) && boardColor[sq + pieceDeltas[PAWN][dirMod]] == (sideToMove*(-1))) {
						if ((sideToMove == WHITE && sq / 16 == 6) || (sideToMove == BLACK && sq / 16 == 1)) {
							movesInPosition = addPromotionPermutations(moves, movesInPosition, sq, sq + pieceDeltas[PAWN][dirMod], board[sq + pieceDeltas[PAWN][dirMod]], 1);
						}
						else {
							moveAdd(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod], PAWN, board[sq + pieceDeltas[PAWN][dirMod]], 1);
						}
					}
					if (board[sq + pieceDeltas[PAWN][dirMod + 1]] == EMPTY) {
						if ((sideToMove == WHITE && (sq >> 4) == 6) || (sideToMove == BLACK && (sq >> 4) == 1)) {
							movesInPosition = addPromotionPermutations(moves, movesInPosition, sq, sq + pieceDeltas[PAWN][dirMod + 1], board[sq + pieceDeltas[PAWN][dirMod + 1]], 0);
						}
						else {
							moveAdd(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 1], PAWN, board[sq + pieceDeltas[PAWN][dirMod + 1]], 0);
						}
					}
					if (ON_BOARD(sq + pieceDeltas[PAWN][dirMod + 2]) && boardColor[sq + pieceDeltas[PAWN][dirMod + 2]] == (sideToMove*(-1))) {
						if ((sideToMove == WHITE && sq / 16 == 6) || (sideToMove == BLACK && sq / 16 == 1)) {
							movesInPosition = addPromotionPermutations(moves, movesInPosition, sq, sq + pieceDeltas[PAWN][dirMod + 2], board[sq + pieceDeltas[PAWN][dirMod + 2]], 1);
						}
						else {
							moveAdd(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 2], PAWN, board[sq + pieceDeltas[PAWN][dirMod + 2]], 1);
						}
					}
					if (enPassant != -1) {
						if ((sq + pieceDeltas[PAWN][dirMod]) == enPassant) {
							moveAdd(moves, movesInPosition, sq, sq + pieceDeltas[PAWN][dirMod], PAWN, PAWN, 1);
							moves[movesInPosition++] |= (1 << MOVE_EN_PASSANT_SHIFT);
						}
						else if (sq + pieceDeltas[PAWN][dirMod + 2] == enPassant) {
							moveAdd(moves, movesInPosition, sq, sq + pieceDeltas[PAWN][dirMod + 2], PAWN, PAWN, 1);
							moves[movesInPosition++] |= (1 << MOVE_EN_PASSANT_SHIFT);
						}
					}
				}
				else { // non sliding piece
					for (int dir = 0; dir < 8; dir++) {
						newPos = sq + pieceDeltas[board[sq]][dir];
						if (ON_BOARD(newPos)) {
							if (boardColor[newPos] == sideToMove*(-1)) {
								moveAdd(moves, movesInPosition++, sq, newPos, board[sq], board[newPos], 1);
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
								moveAdd(moves, movesInPosition++, sq, newPos, board[sq], board[newPos], 1);
							}
							break;
						}
						moveAdd(moves, movesInPosition++, sq, newPos, board[sq], EMPTY, 0);
					}
				}
			}
		}
	}

	return movesInPosition;
}

int Board::getLegalMovesInCheck(int *moves) {
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
					moveAdd(moves, numOfMoves++, kingSq, newPos, KING, board[newPos], 1);
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
		for (int pieceType = 1; pieceType < 6; pieceType++) {
			for (int pieceIndex = 1; pieceIndex <= pieceLists[(sideToMove == WHITE) ? 6 + pieceType : pieceType][COUNT]; pieceIndex++) {
				int sq = pieceLists[(sideToMove == WHITE) ? 6 + pieceType : pieceType][pieceIndex];
				if (pieceType == PAWN) {
					int dirMod = (sideToMove == WHITE) ? 0 : 4;
					if (numOfLegalSquares > 1) {
						if (((sideToMove == WHITE && sq / 16 == 1) || (sideToMove == BLACK && sq / 16 == 6)) && board[(sq + pieceDeltas[PAWN][dirMod + 1])] == EMPTY) {
							if (std::any_of(legalSquares, legalSquares + numOfLegalSquares - 1, [=](int i) {return i == sq + pieceDeltas[PAWN][dirMod + 3]; })) {
								moveAdd(moves, numOfMoves++, sq, sq + pieceDeltas[PAWN][dirMod + 3], PAWN, EMPTY, 0);
							}
						}
						if (std::any_of((legalSquares), legalSquares + numOfLegalSquares - 1, [=](int i) {return i == sq + pieceDeltas[PAWN][dirMod + 1]; })) {
							if ((sideToMove == WHITE && sq / 16 == 6) || (sideToMove == BLACK && sq / 16 == 1)) {
								numOfMoves = addPromotionPermutations(moves, numOfMoves, sq, sq + pieceDeltas[PAWN][dirMod + 1], EMPTY, 0);
							}
							else {
								moveAdd(moves, numOfMoves++, sq, sq + pieceDeltas[PAWN][dirMod + 1], PAWN, EMPTY, 0);
							}
						}
					}

					if (sq + pieceDeltas[PAWN][dirMod] == attackers[0]) {
						if ((sideToMove == WHITE && sq / 16 == 6) || (sideToMove == BLACK && sq / 16 == 1)) {
							numOfMoves = addPromotionPermutations(moves, numOfMoves, sq, attackers[0], board[attackers[0]], 1);
						}
						else {
							moveAdd(moves, numOfMoves++, sq, attackers[0], PAWN, board[attackers[0]], 1);
						}
					}
					else if (sq + pieceDeltas[PAWN][dirMod + 2] == attackers[0]) {
						if ((sideToMove == WHITE && sq / 16 == 6) || (sideToMove == BLACK && sq / 16 == 1)) {
							numOfMoves = addPromotionPermutations(moves, numOfMoves, sq, attackers[0], board[attackers[0]], 1);
						}
						else {
							moveAdd(moves, numOfMoves++, sq, attackers[0], PAWN, board[attackers[0]], 1);
						}
					}
					else if (enPassant != -1 && (board[attackers[0]] == PAWN || std::any_of(legalSquares, legalSquares + numOfLegalSquares, [=](int i) {return i == enPassant; }))) {
						if (sq + pieceDeltas[PAWN][dirMod] == enPassant) {
							moveAdd(moves, numOfMoves, sq, sq + pieceDeltas[PAWN][dirMod], PAWN, board[attackers[0]], 0);
							moves[numOfMoves++] |= (1 << MOVE_EN_PASSANT_SHIFT);
						}
						else if (sq + pieceDeltas[PAWN][dirMod + 2] == enPassant) {
							moveAdd(moves, numOfMoves, sq, sq + pieceDeltas[PAWN][dirMod + 2], PAWN, board[attackers[0]], 1);
							moves[numOfMoves++] |= (1 << MOVE_EN_PASSANT_SHIFT);
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
					if (pieceType <= 2) {
						for (int dir = 0; dir < 8; dir++) {
							if (std::any_of(legalSquares, legalSquares + numOfLegalSquares, [=](int i) {return i == sq + pieceDeltas[board[sq]][dir]; })) {
								if (sq + pieceDeltas[board[sq]][dir] == attackers[0]) {
									moveAdd(moves, numOfMoves++, sq, sq + pieceDeltas[board[sq]][dir], board[sq], board[attackers[0]], 1);
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
										moveAdd(moves, numOfMoves++, sq, newPos, board[sq], board[newPos], 1);
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

	return numOfMoves;
}

//get position of first piece in a given direction from a given square
int Board::getSqOfFirstPieceOnRay(int fromSq, int ray) {
	int newPos = fromSq;
	while (((newPos += ray) & 0x88) == 0) {
		if (board[newPos] != EMPTY) {
			return newPos;
		}
	}

	return -1;
}

// get all capture and promotion moves
int Board::getQuiescenceMoves(int *moves) {
	int movesInPosition = 0;
	int newPos;

	for (int pieceType = 0; pieceType < 6; pieceType++) {
		for (int pieceIndex = 1; pieceIndex <= pieceLists[(sideToMove == WHITE) ? 6 + pieceType : pieceType][COUNT]; pieceIndex++) {
			int sq = pieceLists[(sideToMove == WHITE) ? 6 + pieceType : pieceType][pieceIndex];
			if (pieceType <= 2) {
				if (pieceType == PAWN) {
					int dirMod = (sideToMove == WHITE) ? 0 : 4;
					if ((sideToMove == WHITE && sq / 16 == 6) || (sideToMove == BLACK && sq / 16 == 1)) {
						if (board[sq + pieceDeltas[PAWN][dirMod + 1]] == EMPTY) {
							movesInPosition = addPromotionPermutations(moves, movesInPosition, sq, sq + pieceDeltas[PAWN][dirMod + 1], EMPTY, 0);
						}
					}
					if (ON_BOARD(sq + pieceDeltas[PAWN][dirMod]) && boardColor[sq + pieceDeltas[PAWN][dirMod]] == (sideToMove*(-1))) {
						if ((sideToMove == WHITE && sq / 16 == 6) || (sideToMove == BLACK && sq / 16 == 1)) {
							movesInPosition = addPromotionPermutations(moves, movesInPosition, sq, sq + pieceDeltas[PAWN][dirMod], board[sq + pieceDeltas[PAWN][dirMod]], 1);
						}
						else {
							moveAdd(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod], PAWN, board[sq + pieceDeltas[PAWN][dirMod]], 1);
						}
					}
					if (ON_BOARD(sq + pieceDeltas[PAWN][dirMod + 2]) && boardColor[sq + pieceDeltas[PAWN][dirMod + 2]] == (sideToMove*(-1))) {
						if ((sideToMove == WHITE && sq / 16 == 6) || (sideToMove == BLACK && sq / 16 == 1)) {
							movesInPosition = addPromotionPermutations(moves, movesInPosition, sq, sq + pieceDeltas[PAWN][dirMod + 2], board[sq + pieceDeltas[PAWN][dirMod + 2]], 1);
						}
						else {
							moveAdd(moves, movesInPosition++, sq, sq + pieceDeltas[PAWN][dirMod + 2], PAWN, board[sq + pieceDeltas[PAWN][dirMod + 2]], 1);
						}
					}
					if (enPassant != -1) {
						if ((sq + pieceDeltas[PAWN][dirMod]) == enPassant) {
							moveAdd(moves, movesInPosition, sq, sq + pieceDeltas[PAWN][dirMod], PAWN, PAWN, 1);
							moves[movesInPosition++] |= (1 << MOVE_EN_PASSANT_SHIFT);
						}
						else if (sq + pieceDeltas[PAWN][dirMod + 2] == enPassant) {
							moveAdd(moves, movesInPosition, sq, sq + pieceDeltas[PAWN][dirMod + 2], PAWN, PAWN, 1);
							moves[movesInPosition++] |= (1 << MOVE_EN_PASSANT_SHIFT);
						}
					}
				}
				else {
					for (int dir = 0; dir < 8; dir++) {
						newPos = sq + pieceDeltas[board[sq]][dir];
						if (ON_BOARD(newPos)) {
							if (boardColor[newPos] == sideToMove*(-1)) {
								moveAdd(moves, movesInPosition++, sq, newPos, board[sq], board[newPos], 1);
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
								moveAdd(moves, movesInPosition++, sq, newPos, board[sq], board[newPos], 1);
							}
							break;
						}
					}
				}
			}
		}
	}

	return movesInPosition;
}

//helper function to generate all promotion permutations
inline int Board::addPromotionPermutations(int *moves, int moveNum, int sq, int tarSq, Piece attackedPiece, int capture) {
	moveAdd(moves, moveNum, sq, tarSq, PAWN, attackedPiece, capture);
	moves[moveNum++] |= (QUEEN << MOVE_PROMOTED_TO_SHIFT) | (1 << MOVE_PROMOTION_SHIFT);
	moveAdd(moves, moveNum, sq, tarSq, PAWN, attackedPiece, capture);
	moves[moveNum++] |= (ROOK << MOVE_PROMOTED_TO_SHIFT) | (1 << MOVE_PROMOTION_SHIFT);
	moveAdd(moves, moveNum, sq, tarSq, PAWN, attackedPiece, capture);
	moves[moveNum++] |= (BISHOP << MOVE_PROMOTED_TO_SHIFT) | (1 << MOVE_PROMOTION_SHIFT);
	moveAdd(moves, moveNum, sq, tarSq, PAWN, attackedPiece, capture);
	moves[moveNum++] |= (KNIGHT << MOVE_PROMOTED_TO_SHIFT) | (1 << MOVE_PROMOTION_SHIFT);
	return moveNum;
}

inline void Board::moveAdd(int *moves, int moveNum, int squareFrom, int squareTo, Piece movedPiece, Piece attacked, int capture) {
	moves[moveNum] =	(squareFrom)							|
						(squareTo << MOVE_TO_SQ_SHIFT)			|
						(movedPiece << MOVE_MOVED_PIECE_SHIFT)	|
						(attacked << MOVE_ATTACKED_PIECE_SHIFT)	|
						(capture << MOVE_CAPTURE_SHIFT)
		;
	//moves[moveNum] = Move{ static_cast<uint8_t>(squareFrom), static_cast<uint8_t>(squareTo), movedPiece, attacked, static_cast<uint16_t>(flags), static_cast<uint8_t>(moveNum) };
}

void Board::moveMakeNull() {
	sideToMove = Color(sideToMove*(-1));
	zobristKey ^= zobrist.side;
	halfMoveCount++;
	history[++historyIndex] = State{ -1, wCastlingRights, bCastlingRights, enPassant, halfMoveClk };
	if (enPassant != -1) {
		zobristKey ^= zobrist.enPassant[enPassant % 8];
		enPassant = -1;
	}
}

void Board::moveUnmakeNull() {
	State prevState = history[historyIndex--];
	sideToMove = Color(sideToMove * (-1));
	zobristKey ^= zobrist.side;
	halfMoveCount--;
	if (enPassant != -1) {
		zobristKey ^= zobrist.enPassant[enPassant % 8];
	}
	enPassant = prevState.enPassant;
	if (enPassant != -1) {
		zobristKey ^= zobrist.enPassant[enPassant % 8];
	}
}

void Board::moveMake(int move) { // TODO maybe consider writing captured piece here instead of in each move add if that is more efficient
	history[++historyIndex] = State{ move, wCastlingRights, bCastlingRights, enPassant, halfMoveClk };
	repStack[++repIndex] = zobristKey;

	int fromSq = move & MOVE_FROM_SQ_MASK;
	int toSq = ((move & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT);
	Piece movedPiece = Piece((move & MOVE_MOVED_PIECE_MASK) >> MOVE_MOVED_PIECE_SHIFT);
	Piece attackedPiece = Piece((move & MOVE_ATTACKED_PIECE_MASK) >> MOVE_ATTACKED_PIECE_SHIFT);

	if (movedPiece == KING) {
		if (boardColor[fromSq] == WHITE) {
			wKingSq = toSq;
		}
		else {
			bKingSq = toSq;
		}
	}
	zobristKey ^= zobrist.wCastlingRights[wCastlingRights];
	zobristKey ^= zobrist.bCastlingRights[bCastlingRights];
	if (enPassant != -1) zobristKey ^= zobrist.enPassant[enPassant % 8];

	switch (fromSq) {
		case 0: wCastlingRights &= ~CASTLE_LONG; break;
		case 4: wCastlingRights &= ~CASTLE_BOTH; break;
		case 7: wCastlingRights &= ~CASTLE_SHORT; break;
		case 112: bCastlingRights &= ~CASTLE_LONG; break;
		case 116: bCastlingRights &= ~CASTLE_BOTH; break;
		case 119: bCastlingRights &= ~CASTLE_SHORT; break;
	} 
	switch (toSq){
		case 0: wCastlingRights &= ~CASTLE_LONG; break;
		case 4: wCastlingRights &= ~CASTLE_BOTH; break;
		case 7: wCastlingRights &= ~CASTLE_SHORT; break;
		case 112: bCastlingRights &= ~CASTLE_LONG; break;
		case 116: bCastlingRights &= ~CASTLE_BOTH; break;
		case 119: bCastlingRights &= ~CASTLE_SHORT; break;
	}

	if (move & MOVE_CAPTURE_MASK) {
		halfMoveClk = -1;
		materialTotal += (pieceValues[attackedPiece] * sideToMove);
		if (move & MOVE_EN_PASSANT_MASK) {
			if ((toSq % 8) - (fromSq % 8) == WEST) {
				clearSq(fromSq + WEST);
				pieceListRemovePiece(fromSq + WEST, PAWN, Color(sideToMove * (-1)));
			}
			else {
				clearSq(fromSq + EAST);
				pieceListRemovePiece(fromSq + EAST, PAWN, Color(sideToMove * (-1)));
			}
		}
		else {
			clearSq(toSq);
			pieceListRemovePiece(toSq, attackedPiece, Color(sideToMove * (-1)));
		}
	}

	if (movedPiece == PAWN) {
		halfMoveClk = -1;
		if (abs(toSq - fromSq) == NN) { // pawn double move
			enPassant = (fromSq + toSq) / 2;
			zobristKey ^= zobrist.enPassant[enPassant % 8];
		}
		else if (enPassant != -1) {
			enPassant = -1;
		}
	}
	else if (enPassant != -1) {
		enPassant = -1;
	}

	if (move & MOVE_PROMOTION_MASK) {
		pieceListRemovePiece(fromSq, PAWN, sideToMove);
		Piece promotedTo = Piece(((move & MOVE_PROMOTED_TO_MASK) >> MOVE_PROMOTED_TO_SHIFT));
		materialTotal += (pieceValues[promotedTo] - pieceValues[PAWN])*sideToMove;
		clearSq(fromSq);
		setSq(fromSq, promotedTo, sideToMove);
		pieceListAddPiece(fromSq, promotedTo, sideToMove);
		movedPiece = promotedTo;
	}

	else if (move & MOVE_CASTLE_LONG_MASK) {
		pieceListMove(toSq + WEST * 2, toSq + EAST, ROOK, sideToMove);
		setSq(toSq + EAST, ROOK, sideToMove);
		clearSq(toSq + WEST * 2);
		
		if (sideToMove == WHITE) {
			wCastlingRights = 0;
		}
		else {
			bCastlingRights = 0;
		}
	}
	else if (move & MOVE_CASTLE_SHORT_MASK) {
		pieceListMove(toSq + EAST, toSq + WEST, ROOK, sideToMove);
		setSq(toSq + WEST, ROOK, sideToMove);
		clearSq(toSq + EAST);

		if (sideToMove == WHITE) {
			wCastlingRights = 0;
		}
		else {
			bCastlingRights = 0;
		}
	}

	pieceListMove(fromSq, toSq, movedPiece, sideToMove);
	setSq(toSq, board[fromSq], boardColor[fromSq]);
	clearSq(fromSq);


	halfMoveCount++;
	halfMoveClk++;
	sideToMove = Color(sideToMove*(-1));
	zobristKey ^= zobrist.side;
	zobristKey ^= zobrist.wCastlingRights[wCastlingRights];
	zobristKey ^= zobrist.bCastlingRights[bCastlingRights];
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

	int fromSq = prevState.move & MOVE_FROM_SQ_MASK;
	int toSq = ((prevState.move & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT);
	Piece movedPiece = Piece(((prevState.move & MOVE_MOVED_PIECE_MASK) >> MOVE_MOVED_PIECE_SHIFT));
	Piece attackedPiece = Piece(((prevState.move & MOVE_ATTACKED_PIECE_MASK) >> MOVE_ATTACKED_PIECE_SHIFT));

	sideToMove = Color(sideToMove*(-1));
	zobristKey ^= zobrist.side;
	if (enPassant != -1) zobristKey ^= zobrist.enPassant[enPassant % 8];
	zobristKey ^= zobrist.wCastlingRights[wCastlingRights];
	zobristKey ^= zobrist.bCastlingRights[bCastlingRights];

	if (movedPiece == KING) {
		if (boardColor[toSq] == WHITE) {
			wKingSq = fromSq;
		}
		else {
			bKingSq = fromSq;
		}
		if (prevState.move & MOVE_CASTLE_LONG_MASK) {
			pieceListMove(toSq + EAST, toSq + 2 * WEST, ROOK, sideToMove);
			setSq(toSq + 2 * WEST, ROOK, sideToMove);
			clearSq(toSq + EAST);
		}
		else if (prevState.move & MOVE_CASTLE_SHORT_MASK) {
			pieceListMove(toSq + WEST, toSq + EAST, ROOK, sideToMove);
			setSq(toSq + EAST, ROOK, sideToMove);
			clearSq(toSq + WEST);
		}
	}
	if (prevState.move & MOVE_PROMOTION_MASK) {
		Piece promotedTo = Piece(((prevState.move & MOVE_PROMOTED_TO_MASK) >> MOVE_PROMOTED_TO_SHIFT));
		materialTotal -= (pieceValues[promotedTo] - pieceValues[PAWN]) * sideToMove;
		pieceListRemovePiece(toSq, promotedTo, sideToMove);
		pieceListAddPiece(toSq, PAWN, sideToMove);
	}
	pieceListMove(toSq, fromSq, movedPiece, sideToMove);
	clearSq(toSq); // must clear first to remove position value of piece
	if (prevState.move & MOVE_CAPTURE_MASK) {
		materialTotal -= (pieceValues[attackedPiece] * sideToMove);
		if (prevState.move & MOVE_EN_PASSANT_MASK) {
			if ((toSq % 8) - (fromSq % 8) == WEST) {
				setSq(fromSq + WEST, PAWN, Color(sideToMove*(-1)));
				pieceListAddPiece(fromSq + WEST, PAWN, Color(sideToMove*(-1)));
			}
			else {
				setSq(fromSq + EAST, PAWN, Color(sideToMove*(-1)));
				pieceListAddPiece(fromSq + EAST, PAWN, Color(sideToMove*(-1)));
			}
			clearSq(toSq);
		}
		else {
			setSq(toSq, attackedPiece, Color(sideToMove*(-1)));
			pieceListAddPiece(toSq, attackedPiece, Color(sideToMove*(-1)));
		}
	}

	setSq(fromSq, movedPiece, sideToMove);

	wCastlingRights = prevState.wCastleRights;
	bCastlingRights = prevState.bCastleRights;
	enPassant = prevState.enPassant;
	halfMoveClk = prevState.halfMoveClk;
	halfMoveCount--;
	if (enPassant != -1) zobristKey ^= zobrist.enPassant[enPassant % 8];
	zobristKey ^= zobrist.wCastlingRights[wCastlingRights];
	zobristKey ^= zobrist.bCastlingRights[bCastlingRights];

	--repIndex;
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
		positionTotal += side * tablePtr[sq];
		int sqSide = side == WHITE ? 1 : 0;
		zobristKey ^= zobrist.pieces[piece][sqSide][sq];
	}
	board[sq] = piece;
	boardColor[sq] = side;
}

inline void Board::pieceListRemovePiece(int sq, Piece piece, Color side) {
	if (boardPieceIndex[sq] != pieceLists[(side == WHITE) ? 6 + piece : piece][COUNT]) {
		std::swap(pieceLists[(side == WHITE) ? 6 + piece : piece][pieceLists[(side == WHITE) ? 6 + piece : piece][COUNT]], pieceLists[(side == WHITE) ? 6 + piece : piece][boardPieceIndex[sq]]);
		std::swap(boardPieceIndex[sq], boardPieceIndex[pieceLists[(side == WHITE) ? 6 + piece : piece][boardPieceIndex[sq]]]);
		// to ensure no "holes" in piece list, we swap the element to be deleted with the last, and delete the last entry
	}
	pieceLists[(side == WHITE) ? 6 + piece : piece][boardPieceIndex[sq]] = -1;
	pieceLists[(side == WHITE) ? 6 + piece : piece][COUNT]--;
	boardPieceIndex[sq] = -1;
}

inline void Board::pieceListAddPiece(int sq, Piece piece, Color side) {
	pieceLists[(side == WHITE) ? 6 + piece : piece][++pieceLists[(side == WHITE) ? 6 + piece : piece][COUNT]] = sq;
	boardPieceIndex[sq] = pieceLists[(side == WHITE) ? 6 + piece : piece][COUNT];
}

inline void Board::pieceListMove(int fromSq, int toSq, Piece piece, Color side) {
	pieceLists[(side == WHITE) ? 6 + piece : piece][boardPieceIndex[fromSq]] = toSq;
	boardPieceIndex[toSq] = boardPieceIndex[fromSq];
	boardPieceIndex[fromSq] = -1;
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
	for (int pieceType = 0; pieceType < 6; pieceType++) {
		for (int pieceIndex = 1; pieceIndex <= pieceLists[(attackColor == WHITE) ? 6 + pieceType : pieceType][COUNT]; pieceIndex++) {
			int sq = pieceLists[(attackColor == WHITE) ? 6 + pieceType : pieceType][pieceIndex];
			if (sq != chkdSq && sq != ignoreAttackerOnSq) {
				lookupVal = chkdSq - sq + 128;
				if (pieceType == PAWN) {
					int attackGroup = (attackColor == WHITE) ? attackGroups[PAWN] - 8 : attackGroups[PAWN] - 4;
					if (attackGroup & attackArray[lookupVal]) {
						isAttacked = true;
						break;
					}
				}
				else {
					if (attackGroups[pieceType] & attackArray[lookupVal]) { // piece can attack sq
						if (pieceType <= 2) {
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
	for (int pieceType = 0; pieceType < 6; pieceType++) {
		for (int pieceIndex = 1; pieceIndex <= pieceLists[(attackColor == WHITE) ? 6 + pieceType : pieceType][COUNT]; pieceIndex++) {
			int sq = pieceLists[(attackColor == WHITE) ? 6 + pieceType : pieceType][pieceIndex];
			if (sq != chkdSq) {
				lookupVal = chkdSq - sq + 128;
				if (pieceType == PAWN) {
					int attackGroup = (attackColor == WHITE) ? attackGroups[PAWN] - 8 : attackGroups[PAWN] - 4;
					if (attackGroup & attackArray[lookupVal]) attackingSquares[numOfAttackers++] = sq;
				}
				else {
					if (attackArray[lookupVal] & attackGroups[pieceType]) { // piece can attack sq
						if (pieceType <= 2) {
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
	}

	return numOfAttackers;
}

void Board::printMoves(int *moves, int numOfMoves) {
	int pieceNameLength = 10;
	int moveLength = 8;
	int numberLength = 4;

	std::cout << std::left << std::setw(numberLength) << std::setfill(' ') << "#";
	std::cout << std::left << std::setw(moveLength) << std::setfill(' ') << "Move";
	std::cout << std::left << std::setw(pieceNameLength) << std::setfill(' ') << "Moved";
	std::cout << std::left << std::setw(pieceNameLength) << std::setfill(' ') << "Attacked";
	std::cout << "Special" << std::endl;

	for (int i = 0; i < numOfMoves; i++) {
		std::cout << std::left << std::setw(numberLength) << std::setfill(' ') << i;
		printMove(moves[i]);
	}
}

void Board::printMove(int move) {
	int pieceNameLength = 10;
	int moveLength = 8;

	bool firstSpecial = true;

	std::stringstream moveSS;
	moveSS << SQ_FILE((move & MOVE_FROM_SQ_MASK)) << SQ_RANK((move & MOVE_FROM_SQ_MASK)) << SQ_FILE(((move & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT)) << SQ_RANK(((move & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT));
	std::cout << std::left << std::setw(moveLength) << std::setfill(' ') << moveSS.str();

	switch ((move & MOVE_MOVED_PIECE_MASK) >> MOVE_MOVED_PIECE_SHIFT) {
		case PAWN:
			std::cout << std::left << std::setw(pieceNameLength) << std::setfill(' ') << "Pawn";
			break;
		case KNIGHT:
			std::cout << std::left << std::setw(pieceNameLength) << std::setfill(' ') << "Knight";
			break;
		case BISHOP:
			std::cout << std::left << std::setw(pieceNameLength) << std::setfill(' ') << "Bishop";
			break;
		case ROOK:
			std::cout << std::left << std::setw(pieceNameLength) << std::setfill(' ') << "Rook";
			break;
		case QUEEN:
			std::cout << std::left << std::setw(pieceNameLength) << std::setfill(' ') << "Queen";
			break;
		case KING:
			std::cout << std::left << std::setw(pieceNameLength) << std::setfill(' ') << "King";
			break;
	}
	if (move & MOVE_CAPTURE_MASK) {
		switch ((move & MOVE_ATTACKED_PIECE_MASK) >> MOVE_ATTACKED_PIECE_SHIFT) {
			case PAWN:
				std::cout << std::left << std::setw(pieceNameLength) << std::setfill(' ') << "Pawn";
				break;
			case KNIGHT:
				std::cout << std::left << std::setw(pieceNameLength) << std::setfill(' ') << "Knight";
				break;
			case BISHOP:
				std::cout << std::left << std::setw(pieceNameLength) << std::setfill(' ') << "Bishop";
				break;
			case ROOK:
				std::cout << std::left << std::setw(pieceNameLength) << std::setfill(' ') << "Rook";
				break;
			case QUEEN:
				std::cout << std::left << std::setw(pieceNameLength) << std::setfill(' ') << "Queen";
				break;
			case KING:
				std::cout << std::left << std::setw(pieceNameLength) << std::setfill(' ') << "King";
				break;
		}
	}
	else {
		std::cout << std::left << std::setw(pieceNameLength) << std::setfill(' ') << "N/A";
	}

	if (move & MOVE_EN_PASSANT_MASK) {
		if (!firstSpecial)
			std::cout << ", ";
		std::cout << "En Passant";
	}

	if (move & MOVE_PROMOTION_MASK) {
		if (!firstSpecial)
			std::cout << ", ";
		std::cout << "Promotion to ";
		switch ((move & MOVE_PROMOTED_TO_MASK) >> MOVE_PROMOTED_TO_SHIFT) {
		case QUEEN:
			std::cout << "Queen";
			break;
		case ROOK:
			std::cout << "Rook";
			break;
		case BISHOP:
			std::cout << "Bishop";
			break;
		case KNIGHT:
			std::cout << "Knight";
			break;
		}
	}

	if (move & MOVE_CASTLE_LONG_MASK) {
		if (!firstSpecial)
			std::cout << ", ";
		std::cout << "Castle long";
	}
	else if (move & MOVE_CASTLE_SHORT_MASK) {
		if (!firstSpecial)
			std::cout << ", ";
		std::cout << "Castle short";
	}
	std::cout << std::endl;
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
			std::cout << "\t";
			for (int i = 8; i > 0; i--) {
				if (boardPieceIndex[sq - i] == -1)
					std::cout << "0";
				else std::cout << boardPieceIndex[sq - i];
				std::cout << " ";
			}
			std::cout << std::endl;
			sq -= 24;
		}
	}
	std::cout << std::endl;
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
	if (!castling) fen += "-";

	if (enPassant != -1) {
		fen += " ";
		fen += char((enPassant % 8) + 97) + std::to_string((enPassant / 16) + 1);
		fen += " ";
	}
	else {
		fen += " - ";
	}

	fen += std::to_string(halfMoveClk);

	fen += " ";

	fen += std::to_string(int(halfMoveCount / 2));

	return fen;
}

// function taken from Sungorus chess engine
inline unsigned long long Board::rand64() {
	static unsigned long long next = 1;

	next = next * 1103515245 + 12345;
	return next;
}