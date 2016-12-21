// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

#define NW		15	
#define NORTH	16
#define NN		32
#define NE		17
#define EAST	1
#define SE		-15
#define SOUTH	-16
#define SS		-32
#define SW		-17
#define WEST	-1



enum Piece{KING, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, EMPTY};
enum Color {WHITE = 1, BLACK = -1, NONE = 0};

int piece_deltas[6][8] = {
	{NW, NORTH, NE, EAST, SE, SOUTH, SW, WEST}, // king
	{NORTH, NW, NE, NN, SOUTH, SE, SW, SS},		// pawn
	{31, 33, 18, -14, -31, -33, -18, 14},		// knight
	{NW, SW, NE, SE, 0, 0, 0, 0},				// bishop
	{NORTH, EAST, SOUTH, WEST, 0, 0, 0, 0},		// rook
	{NW, NORTH, NE, EAST, SE, SOUTH, SW, WEST}	// queen
};



// TODO: reference additional headers your program requires here
