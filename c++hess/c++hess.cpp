// c++hess.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Board.h"
#include <iostream>
#include <string>


int main()
{	
	Board myboard = Board();

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

	myboard.moveUnmake();
	myboard.printBoard();

	myboard.moveUnmake();
	myboard.printBoard();

	myboard.moveUnmake();
	myboard.printBoard();

	myboard.moveUnmake();
	myboard.printBoard();

	numOfMoves = myboard.getLegalMoves(moves);
	myboard.printMoves(moves, numOfMoves);
	std::cout << myboard.getFenString() << std::endl;

	myboard.moveMake(moves[8]);
	myboard.printBoard();

	myboard.moveUnmake();
	myboard.printBoard();

	std::cin.get();
}

