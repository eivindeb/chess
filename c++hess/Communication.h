#pragma once
#include "Engine.h"
#include <windows.h>

class Engine;

class Com {
	public:
		Com();
		int pipe;
		int debug;
		int xboard(std::string command);
		int receive(Engine *eng);
		void send(std::string command);
		Mode mode;
		int UCI(std::string command, Engine *eng);
		int nothing(std::string command);
		HANDLE hstdin;
		int input();
		Task task;
};