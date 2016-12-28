#include "stdafx.h"
#include "Communication.h"
#include <iostream>
#include <streambuf>
#include <string>

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
	mode = PROTO_XBOARD;
	mode = PROTO_XBOARD;
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

int Com::receive() {

	std::string command;

	if (!input()) return 0;

	/* unwind the search-stack first */
	if (task == TASK_SEARCH) {
		task = TASK_NOTHING;
		return 0;
	}

	std::getline(std::cin, command);

	switch (mode) {
		case PROTO_XBOARD:  xboard(command);  break;
		case PROTO_NOTHING: nothing(command); break;
	}

	return 0;
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
	if (command == "setboard") {

	}

	return 0;
}

int Com::nothing(std::string command) {
	return 0;
}

void Com::send(std::string command) {
	std::cout << command << std::endl;
}

