#include "stdafx.h"
#include <windows.h>
#include "Engine.h"
#include "Communication.h"
#include <iostream>
#include <algorithm>
#include <streambuf>
#include <string>
#include <sstream>
#include <vector>


Engine::Engine(int _sideToPlay, int _depth, std::string fen) : tTable(49999991), timer(20), sideToPlay(_sideToPlay), maxDepth(_depth) {
	if (fen == "") board = Board();
	else board = Board(fen);
	std::thread t1 = std::thread([this] {this->comReceive(); });
	t1.detach();
};

int Engine::alphaBeta(int alpha, int beta, int depthLeft) {
	if (depthLeft == 0) return quiescence(alpha, beta);
	Move moves[218];
	Move ttMove = Move{ (uint8_t) 0, (uint8_t) 0, EMPTY, EMPTY, 0, (uint8_t) NO_ID };
	int score = 0;
	int bestMoveIndex = 0;
	int ttVal = 0;
	TT_FLAG ttFlag = TT_ALPHA;
	if ((ttVal = tTable.probe(board.zobristKey, depthLeft, alpha, beta, &ttMove)) != INVALID) {
		return ttVal;
	}

	int numOfMoves = 0;
	if (board.inCheck(board.sideToMove)) {
		numOfMoves = board.getLegalMovesInCheck(moves);
		if (numOfMoves == 0) {
			return (MATE_SCORE + (maxDepth - depthLeft))*board.sideToMove;
		}
	}
	else {
		numOfMoves = board.getLegalMoves(moves);
		if (numOfMoves == 0) {
			numOfMoves = 0;
		}
	}
	sortMoves(moves, numOfMoves, ttMove.id);
	
	for (int i = 0; i < numOfMoves; i++) {
		board.moveMake(moves[i]);
		if (board.inCheck(Color(board.sideToMove*(-1)))) { // move that puts its own king in check
			board.moveUnmake();
			continue;
		}
		score = -alphaBeta(-beta, -alpha, depthLeft - 1);
		board.moveUnmake();
		if (score > alpha) {
			if (score >= beta) {
				tTable.saveEntry(board.zobristKey, depthLeft, board.halfMoveCount, beta, TT_BETA, moves[i]);
				return beta;
			}
			bestMoveIndex = i;
			ttFlag = TT_EXACT;
			alpha = score;
		}
	}

	tTable.saveEntry(board.zobristKey, depthLeft, board.halfMoveCount, alpha, ttFlag, moves[bestMoveIndex]);
	return alpha;
}

int Engine::quiescence(int alpha, int beta) {
	int standPat = evaluatePosition();
	if (standPat >= beta) {
		return beta;
	}
	if (alpha < standPat) {
		alpha = standPat;
	}

	//TODO might have to check for in check here??
	Move moves[218];
	int score = 0;
	unsigned long long zobristBefore = 0;
	int numOfCaptures = board.getCaptureMoves(moves);
	mvvLva(moves, numOfCaptures);
	for (int i = 0; i < numOfCaptures; i++) {
		if (!(board.posFlag & POS_EG) && !(moves[i].flags & MFLAGS_PROMOTION) && standPat + pieceValues[moves[i].attackedPiece] + 200 < alpha) { // delta pruning
			continue;
		}
		zobristBefore = board.zobristKey;
		board.moveMake(moves[i]);
		if (board.inCheck(Color(board.sideToMove*(-1)))) {
			board.moveUnmake();
			continue;
		}
		score = -quiescence(-beta, -alpha);
		board.moveUnmake();
		if (board.zobristKey != zobristBefore) {
			zobristBefore = 0;
		}

		if (score >= beta) {
			return beta;
		}
		if (score > alpha) {
			alpha = score;
		}
	}

	return alpha;
}

// For perft checking
unsigned long long Engine::perft(int depthLeft) {
	if (depthLeft == 0) return 1;
	unsigned long long nodes = 0;
	unsigned long long nodesLast = 0;
	unsigned long long zobristBefore = 0;
	int materialBefore = 0;
	int posTotBefore = 0;

	Move moves[218];
	int numOfMoves = 0;
	if (board.inCheck(board.sideToMove)) {
		numOfMoves = board.getLegalMovesInCheck(moves);
		if (numOfMoves == 0) {
			return 0;
		}
	}
	else {
		numOfMoves = board.getLegalMoves(moves);
	}

	if (maxDepth == depthLeft) {
		std::cout << "Move\tNodes" << std::endl;
	}

	for (int i = 0; i < numOfMoves; i++) {
		materialBefore = board.materialTotal;
		posTotBefore = board.positionTotal;
		zobristBefore = board.zobristKey;
		
		board.moveMake(moves[i]);
		if (!board.inCheck(Color(board.sideToMove*(-1)))) {
			nodes += perft(depthLeft - 1);
			if (maxDepth == depthLeft) {
				std::cout << SQ_FILE(moves[i].fromSq) << SQ_RANK(moves[i].fromSq) << " " << SQ_FILE(moves[i].toSq) << SQ_RANK(moves[i].toSq) << "\t" << nodes - nodesLast << std::endl;
				nodesLast = nodes;
			}
		}
		board.moveUnmake();
		if (materialBefore != board.materialTotal || posTotBefore != board.positionTotal || zobristBefore != board.zobristKey) {
			materialBefore = 0;
		}
	}

	if (maxDepth == depthLeft) {
		std::cout << "Total nodes: " << nodes << std::endl;
	}

	return nodes;
}

inline void Engine::mvvLva(Move *moves, int numOfMoves) {
	std::sort(moves, moves + numOfMoves, [](Move &lhs, Move &rhs) {	return pieceSorting[lhs.attackedPiece] > pieceSorting[rhs.attackedPiece] ||
																		(pieceSorting[lhs.attackedPiece] == pieceSorting[rhs.attackedPiece] && pieceSorting[lhs.movedPiece] < pieceSorting[rhs.movedPiece]); });
}

int Engine::evaluatePosition() {
	Move moves[218];
	int wNumOfMoves;
	int bNumOfMoves;
	if (board.sideToMove == WHITE) {
		wNumOfMoves = board.getLegalMoves(moves);
		board.sideToMove = Color(board.sideToMove*(-1));
		bNumOfMoves = board.getLegalMoves(moves);
	}
	else {
		bNumOfMoves = board.getLegalMoves(moves);
		board.sideToMove = Color(board.sideToMove*(-1));
		wNumOfMoves = board.getLegalMoves(moves);
	}
	board.sideToMove = Color(board.sideToMove*(-1));
	int score = board.materialTotal + mobilityWeight * (wNumOfMoves - bNumOfMoves) + board.positionTotal;
	if (board.pieceCount[BISHOP + WHITE * 6 + 6] == 2) {
		score += pieceValues[BISHOP] * 0.1;
	}
	if (board.pieceCount[BISHOP] == 2) {
		score -= pieceValues[BISHOP] * 0.1;
	}

	return (score) * board.sideToMove;
}

//TODO, not working for black
int Engine::findBestMove(Move *moves, int numOfMoves, int depth, int alpha, int beta) {
	int score = -200000;
	int prevScore = score;
	int bestId = 0;
	int bestIndex = 0;
	Move ttMove = Move{ (uint8_t)0, (uint8_t)0, EMPTY, EMPTY, 0, (uint8_t) NO_ID };
	tTable.probe(board.zobristKey, depth, alpha, beta, &ttMove);
	sortMoves(moves, numOfMoves, ttMove.id);
	for (int i = 0; i < numOfMoves; i++) {
		if (timer.timesUp == true) {
			return NO_ID;
		}
		board.moveMake(moves[i]);
		if (board.inCheck(Color(board.sideToMove*(-1)))) { // move that puts its own king in check
			board.moveUnmake();
			continue;
		}
		score = -alphaBeta(-beta, -alpha, depth-1);
		board.moveUnmake();
		//std::cout << std::endl << "On move: " << i << "/" << numOfMoves << ":\t" << moves[i].fromSq << " " << moves[i].toSq;
		//std::cout << "\t(" << SQ_FILE(moves[i].fromSq) << SQ_RANK(moves[i].fromSq) << " " << SQ_FILE(moves[i].toSq) << SQ_RANK(moves[i].toSq) << ")\t" << score << std::endl << std::endl;
		if (score > prevScore) {
			prevScore = score;
			bestId = moves[i].id;
			bestIndex = i;
		}
	}

	//std::cout << "Highest score: " << prevScore << std::endl;
	tTable.saveEntry(board.zobristKey, depth, board.halfMoveCount, prevScore, TT_EXACT, moves[bestIndex]);
	//std::cout << "Best was: " << SQ_FILE(moves[bestIndex].fromSq) << SQ_RANK(moves[bestIndex].fromSq) << SQ_FILE(moves[bestIndex].toSq) << SQ_RANK(moves[bestIndex].toSq) << ": " << prevScore << std::endl;
	return bestId;
}

inline void Engine::sortMoves(Move *moves, int numOfMoves, int bestMoveId) {
	if (bestMoveId != NO_ID && bestMoveId != 0) {
		for (int i = 0; i < numOfMoves; i++) {
			if (moves[i].id == bestMoveId) {
				std::swap(moves[0], moves[i]);
				break;
			}
		}
		mvvLva(moves + 1, numOfMoves - 1); // TODO, might not work
	}
	else {
		mvvLva(moves, numOfMoves);
	}
	
}

int Engine::iterativeDeepening(Move *moves, int numOfMoves) {
	int bestMoveIndex = 0;
	int newBest;
	int pvLength;
	int alpha = -200000 * board.sideToMove;
	int beta = 200000 * board.sideToMove;
	int score;
	Move pvMove;
	timer.start(true);
	std::cout << "PV:" << std::endl;
	for (int i = 1; i <= maxDepth; i++) {
		newBest = findBestMove(moves, numOfMoves, i, alpha, beta);
		pvLength = 0;
		for (int j = 0; j < i; j++) {
			score = tTable.probe(board.zobristKey, 0, 0, 0, &pvMove);
			if (score != INVALID) {
				score *= board.sideToMove;
				std::cout << SQ_FILE(pvMove.fromSq) << SQ_RANK(pvMove.fromSq) << SQ_FILE(pvMove.toSq) << SQ_RANK(pvMove.toSq) << " ";
				board.moveMake(pvMove);
				pvLength++;
			}
			else {
				std::cout << "PV cut short at iteration: " << i << std::endl;
				break;
			}
			
		}
		if (score != INVALID) {
			for (int j = maxDepth; j > pvLength; j--) {
				std::cout << "\t";
			}
			std::cout << score << std::endl;
		}
		for (int j = 0; j < pvLength; j++) {
			board.moveUnmake();
		}
		if (newBest == NO_ID) {
			std::cout << "TIMES UP" << std::endl;
			break;
		}
		bestMoveIndex = newBest;
	}

	return bestMoveIndex;
}



void Engine::comInit() {
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

int Engine::comReceive() {
	std::string command;

	if (!comInput()) return 0;


	while (1) {
		std::getline(std::cin, command);

		switch (mode) {
		//case PROTO_NOTHING: nothing(command); break;
		case PROTO_UCI:		comUCI(command);	break;
		}
	}

	return 1;
}

int Engine::comInput() {
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

int Engine::comUCI(std::string command) {
	if (command == "uci") {
		comSend("id name c ++ hess engine");
		comSend("id author Eivindeb");
		comSend("uciok");
	}
	if (command == "isready") {
		comSend("readyok");
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
						comSend("Unexpected third parameter in moveTo, was:");
						comSend(moveTo);
						break;
					}
				}
				uint8_t sqFrom = SQ_STR_TO_INT(moveFrom);
				uint8_t sqTo = SQ_STR_TO_INT(moveTo.substr(0, 2));

				if (board.board[sqTo] != EMPTY) flags += MFLAGS_CPT;
				if (sqTo == board.enPassant) flags += MFLAGS_ENP;
				if (board.board[sqFrom] == PAWN) {
					flags += MFLAGS_PAWN_MOVE;
					if (abs(sqTo - sqFrom) == 32) flags += MFLAGS_PAWN_DOUBLE;
				}

				if (board.board[sqFrom] == KING && abs(sqFrom - sqTo) == 2) {
					if (sqFrom > sqTo) {
						flags += MFLAGS_CASTLE_LONG;
					}
					else {
						flags += MFLAGS_CASTLE_SHORT;
					}
				}

				Move moveObject = { sqFrom, sqTo, board.board[sqFrom], board.board[sqTo], flags, NO_ID };
				board.moveMake(moveObject);
			}
		}
		else if (tokens[0] == "go") {
			return 2;
		}
	}

	return 0;
}

void Engine::comSend(std::string command) {
	std::cout << command << std::endl;
}