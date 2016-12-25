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

#define MFLAGS_CPT			1
#define MFLAGS_ENP			2
#define MFLAGS_PAWN_DOUBLE	4
#define MFLAGS_CASTLE_LONG	8
#define MFLAGS_CASTLE_SHORT	16
#define MFLAGS_PAWN_MOVE	32
#define MFLAGS_KING_MOVE	64

#define CASTLE_BOTH		3
#define CASTLE_SHORT	1
#define CASTLE_LONG		2

#define ON_BOARD(SQ)	(SQ & 0x88) == 0



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

// courtesy of mediocre chess
//maps a piece to attack groups

static int attackGroups[6] = {13, 12, 32, 28, 3, 31};

/*
int ATTACK_NONE = 0;
int ATTACK_KQR = 1;
int ATTACK_QR = 2;
int ATTACK_KQBwP = 4;
int ATTACK_KQBbP = 8;
int ATTACK_QB = 16;
int ATTACK_N = 32;
*/

static int attackArray[257] =
{ 0,0,0,0,0,0,0,0,0,16,0,0,0,0,0,0,2,0,0,0,		//0-19
0,0,0,16,0,0,16,0,0,0,0,0,2,0,0,0,0,0,16,0,		//20-39
0,0,0,16,0,0,0,0,2,0,0,0,0,16,0,0,0,0,0,0,		//40-59
16,0,0,0,2,0,0,0,16,0,0,0,0,0,0,0,0,16,0,0,     //60-79
2,0,0,16,0,0,0,0,0,0,0,0,0,0,16,32,2,32,16,0,   //80-99
0,0,0,0,0,0,0,0,0,0,32,8,1,8,32,0,0,0,0,0,		//100-119
0,2,2,2,2,2,2,1,0,1,2,2,2,2,2,2,0,0,0,0,		//120-139
0,0,32,4,1,4,32,0,0,0,0,0,0,0,0,0,0,0,16,32,    //140-159
2,32,16,0,0,0,0,0,0,0,0,0,0,16,0,0,2,0,0,16,    //160-179
0,0,0,0,0,0,0,0,16,0,0,0,2,0,0,0,16,0,0,0,		//180-199
0,0,0,16,0,0,0,0,2,0,0,0,0,16,0,0,0,0,16,0,     //200-219
0,0,0,0,2,0,0,0,0,0,16,0,0,16,0,0,0,0,0,0,		//220-239
2,0,0,0,0,0,0,16,0,0,0,0,0,0,0,0,0 };			//240-256

static int dirBySquareDiff[239] = {
	NE,0,0,0,0,0,0,NORTH,0,0,0,0,0,0,NW,0,0,NE,0,0,						//0-19
	0,0,0,NORTH,0,0,0,0,0,NW,0,0,0,0,NE,0,0,0,0,NORTH,					//20-39
	0,0,0,0,NW,0,0,0,0,0,0,NE,0,0,0,NORTH,0,0,0,NW,						//40-59
	0,0,0,0,0,0,0,0,NE,0,0,NORTH,0,0,NW,0,0,0,0,0,						//60-79
	0,0,0,0,0,NE,0,NORTH,0,NW,0,0,0,0,0,0,0,0,0,0,						//80-99
	0,0,NE,NORTH,NW,0,0,0,0,0,0,0,EAST,EAST,EAST,EAST,EAST,EAST,EAST,0,	//100-119
	WEST,WEST,WEST,WEST,WEST,WEST,WEST,0,0,0,0,0,0,0,SE,SOUTH,SW,0,0,0,	//120-139
	0,0,0,0,0,0,0,0,0,SE,0,SOUTH,0,SW,0,0,0,0,0,0,						//140-159
	0,0,0,0,SE,0,0,SOUTH,0,0,SW,0,0,0,0,0,0,0,0,SE,						//160-179
	0,0,0,SOUTH,0,0,0,SW,0,0,0,0,0,0,SE,0,0,0,0,SOUTH,					//180-199
	0,0,0,0,SW,0,0,0,0,SE,0,0,0,0,0,SOUTH,0,0,0,0,						//200-219
	0,SW,0,0,SE,0,0,0,0,0,0,SOUTH,0,0,0,0,0,0,SW };						//220-238

static int pieceValues[6] = { 10000, 100, 300, 300, 500, 900 };

struct Move {
	int fromSq;
	int toSq;
	Piece movedPiece;
	Piece attackedPiece;
	int flags;
};

struct State {
	Move move;
	int wCastleRights;
	int bCastleRights;
	int enPassant;
	int halfMoveClk;
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
