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
	Engine myengine = Engine(1, 20, "", false);
	/*Move moves[218];
	int numOfMoves;
	int index;
	std::stringstream moveStream;*/

	while (1) {
		//do nothing
	}

	/*while (1) {
		switch (com.receive()) {
			case 1:
				myengine.board.loadFromFen(START_FEN);
				break;
			case 2:
				com.send("searching for move");
				numOfMoves = myengine.board.getLegalMoves(moves);
				index = myengine.iterativeDeepening(moves, numOfMoves);
				for (int i = 0; i < numOfMoves; i++) {
					if (moves[i].id == index) {
						index = i;
						break;
					}
				}
				moveStream << "bestmove " << SQ_FILE(moves[index].fromSq) << SQ_RANK(moves[index].fromSq) << SQ_FILE(moves[index].toSq) << SQ_RANK(moves[index].toSq);
				com.send(moveStream.str());
				myengine.board.moveMake(moves[index]);
				moveStream.str("");
				moveStream.clear();
				break;
			default:
				break;
		}
	}*/
	Move moves[218];
	int numOfMoves;
	int moveIndex;
	myengine.board.printBoard();
	//std::cout << "Best was: " << myengine.miniMax(3);
	//myengine.perft(6);

	while (1) {
		if (myengine.board.inCheck(myengine.board.sideToMove)) {
			numOfMoves = myengine.board.getLegalMovesInCheck(moves);
			if (numOfMoves == 0) break;
		}
		else {
			numOfMoves = myengine.board.getLegalMoves(moves);
		}
		if (myengine.board.sideToMove == myengine.sideToPlay || myengine.sideToPlay == 2) {
			moveIndex = myengine.iterativeDeepening(moves, numOfMoves);
		}
		else {
			myengine.board.printMoves(moves, numOfMoves);
			std::cin >> moveIndex;
		}
		if (moveIndex == -1) {
			std::cout << "Undid last move" << std::endl;
			myengine.board.moveUnmake();
			myengine.board.printBoard();
		}
		else {
			for (int i = 0; i < numOfMoves; i++) {
				if (moves[i].id == moveIndex) {
					moveIndex = i;
					break;
				}
			}
			myengine.board.moveMake(moves[moveIndex]);
			std::cout << "Move played: " << SQ_FILE(moves[moveIndex].fromSq) << SQ_RANK(moves[moveIndex].fromSq) << " " << SQ_FILE(moves[moveIndex].toSq) << SQ_RANK(moves[moveIndex].toSq) << std::endl;
			myengine.board.printBoard();
		}
	}
	std::string side = (myengine.board.sideToMove == WHITE) ? "Black" : "White";
	std::cout << side << " won in " << myengine.board.halfMoveCount / 2 << " moves!" << std::endl;
	std::cin.get();
}

