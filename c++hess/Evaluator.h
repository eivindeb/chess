#pragma once
#include "EvalTable.h"
#include "Board.h"

class Evaluator {
	public:
		Evaluator(int tableSize=50);
		int evaluatePosition(Board board);
		EvalTable evalTable;
};
