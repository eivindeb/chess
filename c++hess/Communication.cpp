#include "stdafx.h"
#include "Communication.h"
#include "Engine.h"
#include <iostream>
#include <streambuf>
#include <string>
#include <sstream>
#include <vector>

Com::Com() {
	unsigned long dw;
	hstdin = GetStdHandle(STD_INPUT_HANDLE);
	pipe = !GetConsoleMode(hstdin, &dw);
	if (!pipe) {
		SetConsoleMode(hstdin, dw&~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
		FlushConsoleInputBuffer(hstdin);
	}
	else {
		std::cout.rdbuf()->pubsetbuf(NULL, 0);
		std::cin.rdbuf()->pubsetbuf(NULL, 0);
	}

	task = TASK_NOTHING;
	mode = PROTO_UCI;
}

int Com::input() {
	unsigned long dw = 0;

	if (task == TASK_NOTHING) return 1;

	//if (stdin->_cnt > 0) return 1;

	if (pipe) {
		if (!PeekNamedPipe(hstdin, 0, 0, 0, &dw, 0)) return 1;
		return dw;
	}
	else {
		GetNumberOfConsoleInputEvents(hstdin, &dw);
		if (dw > 1) task = TASK_NOTHING;
	}

	return 0;
}

int Com::receive(Engine *eng) {

	std::string command;

	if (!input()) return 0;


	while(1) {
		std::getline(std::cin, command);

		switch (mode) {
			case PROTO_XBOARD:  xboard(command);  break;
			case PROTO_NOTHING: nothing(command); break;
			case PROTO_UCI:		UCI(command, eng);	break;
		}
	}

	return 1;
}

int Com::xboard(std::string command) {
	if (command == "xboard") {
		mode = PROTO_XBOARD;
		std::cout << "received xboard" << std::endl;
	}
	if (command == "new") {
		//loadFromFen(START_FEN);
		std::cout << "received new" << std::endl;
		return 1;
	}
	if (command == "go") {
		return 2;
	}

	return 0;
}

int Com::UCI(std::string command, Engine *eng) {
	if (command == "uci") {
		send("id name c ++ hess engine");
		send("id author Eivindeb");
		send("uciok");
	}
	if (command == "isready") {
		send("readyok");
	}
	if (command == "quit") {
		exit(0);
	}
	else {
		std::string buf; // Have a buffer string
		std::stringstream ss(command); // Insert the string into a stream

		std::vector<std::string> tokens; // Create vector to hold our words

		while (ss >> buf)
			tokens.push_back(buf);
		
		if (tokens[0] == "setposition") {
			if (tokens[1] == "startpos" && tokens.size() == 2) {
				return 1;
			}
			else {
				std::string move = tokens.back();
				std::string moveFrom = move.substr(0, 2);
				std::string moveTo = move.substr(2);
				uint16_t flags = 0;
				if (moveTo.length() == 3) { //promotion
					flags += MFLAGS_PROMOTION;
					switch (moveTo.back()) {
						case 'q':
							flags += MFLAGS_PROMOTION_QUEEN;
							break;
						case 'r':
							flags += MFLAGS_PROMOTION_ROOK;
							break;
						case 'b':
							flags += MFLAGS_PROMOTION_BISHOP;
							break;
						case 'k':
							flags += MFLAGS_PROMOTION_KNIGHT;
							break;
						default:
							send("Unexpected third parameter in moveTo, was:");
							send(moveTo);
							break;
					}
				}
				uint8_t sqFrom = SQ_STR_TO_INT(moveFrom);
				uint8_t sqTo = SQ_STR_TO_INT(moveTo.substr(0, 2));

				if (eng->board.board[sqTo] != EMPTY) flags += MFLAGS_CPT;
				if (sqTo == eng->board.enPassant) flags += MFLAGS_ENP;
				if (eng->board.board[sqFrom] == PAWN) {
					flags += MFLAGS_PAWN_MOVE;
					if (abs(sqTo - sqFrom) == 32) flags += MFLAGS_PAWN_DOUBLE;
				}

				if (eng->board.board[sqFrom] == KING && abs(sqFrom - sqTo) == 2) {
					if (sqFrom > sqTo) {
						flags += MFLAGS_CASTLE_LONG;
					} else {
						flags += MFLAGS_CASTLE_SHORT;
					}
				}
				
				Move moveObject = { sqFrom, sqTo, eng->board.board[sqFrom], eng->board.board[sqTo], flags, NO_ID };
				eng->board.moveMake(moveObject);
			}
		}
		else if (tokens[0] == "go") {
			return 2;
		}
	}

	return 0;
}

int Com::nothing(std::string command) {
	return 0;
}

void Com::send(std::string command) {
	std::cout << command << std::endl;
}

