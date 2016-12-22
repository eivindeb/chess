#pragma once

class Board {
	public:
		Board();
		Piece board[128];
		Color boardColor[128];
		Color sideToMove;
		long history[20]; // max search depth
		int historyIndex;
		int wCastlingRights;
		int bCastlingRights;
		int getLegalMoves(int *moves);
		void printMoves(int *moves, int numOfMoves);
		//int moves[218];				// think 218 is the maximum number of moves possible in a position, move this to engine
		int halfMoveCount;
		int enPassant;
		void printBoard();
		std::string getFenString();
		void moveMake(int move);
		void moveUnmake();
		int materialTotal;
	private:
		void move_push_back(int *moves, int moveNum, int squareFrom, int squareTo, int capture, int attacked, int attacker);
};