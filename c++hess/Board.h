#pragma once

class Board {
	public:
		Board();
		Piece board[128];
		Color boardColor[128];
		Color sideToMove;
		State history[20]; // max search depth
		int historyIndex;
		int wCastlingRights;
		int bCastlingRights;
		int getLegalMoves(Move *moves);
		int getLegalMovesInCheck(Move *moves);
		int getKingInCheckMoves(Move *moves, int kingSq);
		void printMoves(Move *moves, int numOfMoves);
		//int moves[218];				// think 218 is the maximum number of moves possible in a position, move this to engine
		int halfMoveCount;
		int enPassant;
		void printBoard();
		std::string getFenString();
		void moveMake(Move move);
		void moveUnmake();
		int materialTotal;
		bool sqIsAttacked(int ckdSq, int xRaySq); // TODO this can be expanded to give number of attackers etc.
		int getSqAttackers(int *attackingSq, int chkdSq);
	private:
		void move_add_if_legal(Move *moves, int moveNum, int squareFrom, int squareTo, Piece movedPiece, Piece attacked, int flags);
		int wKingSq;
		int bKingSq;
};