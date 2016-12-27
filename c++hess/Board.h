#pragma once

class Board {
	public:
		Board(std::string fen = "false");
		Piece board[128];
		Color boardColor[128];
		Color sideToMove;
		State history[50]; // max search depth
		int historyIndex;
		int wCastlingRights;
		int bCastlingRights;
		int getLegalMoves(Move *moves);
		int getLegalMovesInCheck(Move *moves);
		void printMoves(Move *moves, int numOfMoves);
		int halfMoveCount;
		int halfMoveClk;
		int enPassant;
		void printBoard();
		std::string getFenString();
		void moveMake(Move move);
		void moveUnmake();
		int materialTotal;
		bool sqIsAttacked(int ckdSq, Color attackColor, int xRaySq = -1, int ignoreAttackerOnSq = -1); // TODO this can be expanded to give number of attackers etc.
		int getSqAttackers(int *attackingSquares, int chkdSq, Color attackColor);
		int addPromotionPermutations(Move *moves, int moveNum, int sq, int tarSq, Piece attackedPiece, int flags);
		bool inCheck(Color side);
	private:
		void move_add_if_legal(Move *moves, int moveNum, int squareFrom, int squareTo, Piece movedPiece, Piece attacked, int flags);
		int wKingSq;
		int bKingSq;
};