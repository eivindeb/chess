#pragma once
#include "stdafx.h"

class Board {
	public:
		Board();
		Piece board[128];
		Color boardColor[128];
		Color sideToMove;
		int getLegalMoves(int *moves);
		//int moves[218];				// think 218 is the maximum number of moves possible in a position, move this to engine
		int halfMoveCount;
		int enPassant[8];				// an array with movecount of double moves, i.e. the pawn can be enPassanted on movecount + 1
	private:
		void move_push_back(int *moves, int moveNum, int squareFrom, int squareTo, int capture, int attacked, int attacker);
};