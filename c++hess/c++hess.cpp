// c++hess.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Board.h"
#include "Engine.h"
#include <iostream>
#include <string>


int main()
{	
	/*
	Board myboard = Board("r1b1kbnr/pppppppp/8/6R1/8/B4n2/PP1q3P/1N2K1NR w KQkq -");
	Move moves[218];
	int numOfMoves = myboard.getLegalMovesInCheck(moves);
	myboard.printBoard();
	myboard.printMoves(moves, numOfMoves);
	std::cin.get();
	*/

	
	Engine myengine = Engine(1, 6);
	myengine.playGame();
}

