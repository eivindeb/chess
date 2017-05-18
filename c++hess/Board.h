#pragma once

class Board {
	public:
		Board(std::string fen = "false");
		int getSqOfFirstPieceOnRay(int fromSq, int ray);
		Piece board[128];
		Color boardColor[128];
		int boardPieceIndex[128];
		Color sideToMove;
		State history[1024]; // max search depth
		int historyIndex;
		int wCastlingRights;
		int bCastlingRights;
		int getQuiescenceMoves(int *moves);
		int getLegalMoves(int *moves);
		int getLegalMovesInCheck(int *moves);
		void printMoves(int *moves, int numOfMoves);
		int halfMoveCount;
		int halfMoveClk;
		int enPassant;
		void printBoard(bool withIndexBoard=false);
		void printMove(int move);
		std::string getFenString();
		void moveMake(int move);
		void moveMakeNull();
		void moveUnmakeNull();
		void moveUnmake();
		int materialTotal;
		int positionTotal;
		bool sqIsAttacked(int ckdSq, Color attackColor, int xRaySq = -1, int ignoreAttackerOnSq = -1); // TODO this can be expanded to give number of attackers etc.
		int getSqAttackers(int *attackingSquares, int chkdSq, Color attackColor);
		int addPromotionPermutations(int *moves, int moveNum, int sq, int tarSq, Piece attackedPiece, int capture);
		bool inCheck(Color side);
		int getSideMaterialValue(Color side);
		int phaseFlag;
		int calculatePositionTotal();
		void loadFromFen(std::string fen);
		Zobrist zobrist;
		unsigned long long zobristKey;
		unsigned long long getZobristKey();
		unsigned long long repStack[1024];
		int	repIndex;
		int pieceLists[12][11];
	private:
		void moveAdd(int *moves, int moveNum, int squareFrom, int squareTo, Piece movedPiece, Piece attacked, int capture);
		int wKingSq;
		int bKingSq;
		void setPiecePositionTotal();
		void clearSq(int sq);
		void setSq(int sq, Piece piece, Color side);
		void pieceListRemovePiece(int sq, Piece piece, Color side);
		void pieceListAddPiece(int sq, Piece piece, Color side);
		void pieceListMove(int fromSq, int toSq, Piece piece, Color side);
		unsigned long long rand64();
};