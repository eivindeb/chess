#pragma once
#include <windows.h>

class Com {
	public:
		Com();
		int pipe;
		int debug;
		int receive();
		void send(std::string command);
		Mode mode;
		int xboard(std::string command);
		int nothing(std::string command);
		HANDLE hstdin;
		int input();
		Task task;
};