// c++hess.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Timer.h"
#include "Board.h"
#include "Engine.h"
#include "Communication.h"
#include <iostream>
#include <string>
#include <sstream>


int main()
{	
	Engine myengine = Engine(1, 20, "");
	/*Move moves[218];
	int numOfMoves;
	int index;
	std::stringstream moveStream;*/

	while (1) {
		//do nothing
	}

	int moves[218];
	int numOfMoves;
	int moveIndex = 0;
	int move;
	//std::cout << "Best was: " << myengine.miniMax(3);a

	/*std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
	std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
	std::cout << duration << std::endl;*/
	

	while (1) {
		if (myengine.board.inCheck(myengine.board.sideToMove)) {
			numOfMoves = myengine.board.getLegalMovesInCheck(moves);
			if (numOfMoves == 0) break;
		}
		else {
			numOfMoves = myengine.board.getLegalMoves(moves);
		}
		if (myengine.board.sideToMove == myengine.sideToPlay || myengine.sideToPlay == 2) {
			move = myengine.iterativeDeepening(moves, numOfMoves, true);
		}
		else {
			myengine.board.printMoves(moves, numOfMoves);
			std::cin >> moveIndex;
			move = moves[moveIndex];
		}
		if (moveIndex == -1) {
			std::cout << "Undid last move" << std::endl;
			myengine.board.moveUnmake();
			myengine.board.printBoard();
		}
		else {
			myengine.board.moveMake(move);
			std::cout << "Move played: " << SQ_FILE(move & MOVE_FROM_SQ_MASK) << SQ_RANK((move & MOVE_FROM_SQ_MASK)) << SQ_FILE(((move & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT)) << SQ_RANK(((move & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT)) << std::endl;
			myengine.board.printBoard();
		}
	}
	std::string side = (myengine.board.sideToMove == WHITE) ? "Black" : "White";
	std::cout << side << " won in " << myengine.board.halfMoveCount / 2 << " moves!" << std::endl;
	std::cin.get();
}

