// c++hess.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Board.h"
#include <iostream>
#include <string>


int main()
{	
	Board myboard = Board();
	int moves[218] = { 0 };
	int numOfMoves = myboard.getLegalMoves(moves);
	myboard.printMoves(moves, numOfMoves);
	myboard.printBoard();
	std::cout << myboard.getFenString() << std::endl;
	std::cin.get();
}

