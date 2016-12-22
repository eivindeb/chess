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

#define MFLAGS_CPT		1
#define MFLAGS_ENP		2
#define CASTLE_BOTH		4
#define CASTLE_SHORT	1
#define CASTLE_LONG		2



enum Piece { KING, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, EMPTY };
enum Color { WHITE = 1, BLACK = -1, NONE = 0 };

static int pieceDeltas[6][8] = {
	{ NW, NORTH, NE, EAST, SE, SOUTH, SW, WEST }, // king
	{ NW, NORTH, NE, NN, SE, SOUTH, SW, SS },		// pawn
	{ 31, 33, 18, -14, -31, -33, -18, 14 },		// knight
	{ NW, SW, NE, SE, 0, 0, 0, 0 },				// bishop
	{ NORTH, EAST, SOUTH, WEST, 0, 0, 0, 0 },		// rook
	{ NW, NORTH, NE, EAST, SE, SOUTH, SW, WEST }	// queen
};

static int pieceValues[6] = { 10000, 100, 300, 300, 500, 900 };

struct Move {
	int fromSq;
	int toSq;
	int movedPiece;
	int attackedPiece;
	int flags;
};

/* move is on the form:
	initial square	(0-6 bit)
	target square	(7-13 bit)
	capture			(14 bit)
	movedpiece		(15-17 bit)
	attacked		(18-20 bit)
	en passant		(21 bit)
	promotion		(22 bit)
	promoted to		(23-25 bit)
	---------------------------
	(in history)
	enPassantbit	(26-33 bit)
	wCastleRights	(34-36 bit)
	bCastleRights	(37-39 bit)
*/

// TODO: reference additional headers your program requires here
