// c++hess.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Board.h"
#include <iostream>
#include <string>


int main()
{	
	/*Board myboard = Board();

	Move moves[218];
	int numOfMoves = myboard.getLegalMoves(moves);
	myboard.printMoves(moves, numOfMoves);
	myboard.printBoard();
	std::cout << myboard.getFenString() << std::endl;
	myboard.moveMake(moves[8]);
	myboard.printBoard();

	std::cout << myboard.getFenString() << std::endl;
	numOfMoves = myboard.getLegalMoves(moves);
	myboard.printMoves(moves, numOfMoves);
	myboard.moveMake(moves[2]);
	myboard.printBoard();

	std::cout << myboard.getFenString() << std::endl;
	numOfMoves = myboard.getLegalMoves(moves);
	myboard.printMoves(moves, numOfMoves);
	myboard.moveMake(moves[22]);
	myboard.printBoard();

	std::cout << myboard.getFenString() << std::endl;
	numOfMoves = myboard.getLegalMoves(moves);
	myboard.printMoves(moves, numOfMoves);
	myboard.moveMake(moves[4]);
	myboard.printBoard();

	std::cout << myboard.getFenString() << std::endl;
	numOfMoves = myboard.getLegalMoves(moves);
	myboard.printMoves(moves, numOfMoves);
	myboard.moveMake(moves[22]);
	myboard.printBoard();

	myboard.moveUnmake();
	myboard.printBoard();

	std::cout << "Square b5 is attacked: " << myboard.sqIsAttacked(65) << std::endl;*/

	Board myboard = Board("r4bkr/ppp3pp/2n1B3/4p3/8/8/PPPP1PPP/RNB1K2R b KQ - 0 3");
	myboard.printBoard();
	Move moves[218];
	int numOfMoves = myboard.getLegalMovesInCheck(moves);
	myboard.printMoves(moves, numOfMoves);

	std::cin.get();
}

