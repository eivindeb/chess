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
#include <math.h>
#include <iomanip>

#define HASH_ORDER		10000000
#define CPT_ORDER		5000
#define PROMOTION_ORDER	5000
#define KILLER_ORDER	3000
#define SEE_ORDER_DEPTH	5

#define EMPTY_MOVE		-1

#define DRAW_OPENING	-10
#define DRAW_ENDGAME	0
#define ENDGAME_MAT		pieceValues[KING] + pieceValues[PAWN] * 4

#define KILLER_PRIMARY		0
#define KILLER_SECONDARY	1

#define DEPTH_TIME_INCREASE 2

#define ALLOW_NULL	true
#define IS_PV		true
#define NOT_PV		false

#define WINDOW_SIZE pieceValues[PAWN] / 2
#define TEMPO		pieceValues[PAWN] / 2

#define R	2	//reduction depth

#define FIXED_SEARCH_DURATION	10*1000;

#define MULTI_PV_LINE_NUM		3

//49999991

Engine::Engine(int _sideToPlay, int _depth, std::string fen) : tTable(4194311), evaluator(50), timer(), sideToPlay(_sideToPlay), maxDepth(_depth) {
	if (fen == "") board = Board();
	else board = Board(fen);
	// Create mutex for board to be used by multithreaded communication
	boardLock = CreateMutex(
		NULL,              // default security attributes
		false,             // initially not owned
		L"LockTest");      // named mutex

	if (boardLock == NULL)
	{
		std::cout << GetLastError() << " Error creating mutex handle" << std::endl;
		exit(0);
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		std::cout << GetLastError() << " Mutex already created" << std::endl;
	}
	if (!console) {
		comInit();
		std::thread t1 = std::thread([this] {this->comReceive(); });
		t1.detach();
	}
	else {
		wmsLeft = -1;
	}
	for (int i = 0; i < 120; i++) {
		for (int j = 0; j < 120; j++) {
			historyMoves[i][j] = 0;
		}
	}
	//wmsLeft = 0;
	//bmsLeft = 0;
};

int Engine::alphaBeta(int alpha, int beta, int depthLeft, int ply, bool allowNull, bool isPV) {
	nodeCount++;
	if (!(nodeCount & 4000) && (timer.timesUp || stopSearch)) {
		return 0;
	}
	bool inCheck = board.inCheck(board.sideToMove);
	if (inCheck) depthLeft++; // in check extension
	if (depthLeft <= 0) return quiescence(alpha, beta);

	int ttMove = EMPTY_MOVE;
	int score = 0;

	if ((score = tTable.probe(board.zobristKey, depthLeft, alpha, beta, &ttMove)) != INVALID) {
		if (!isPV || (score > alpha && score < beta))
			return score;
	}

	if (depthLeft > 2 && allowNull && !(board.phaseFlag & PHASE_EG) && !isPV && !inCheck) { // null move pruning
		board.moveMakeNull();
		score = -alphaBeta(-beta, -beta + 1, depthLeft - 1 - R, ply, !ALLOW_NULL, NOT_PV);
		board.moveUnmakeNull();
		if (score >= beta) return beta;
	}

	bool fPrune = false;

	if (depthLeft == 1 && !inCheck && (evaluator.evaluatePosition(&board) + pieceValues[BISHOP]) < alpha) { // futility pruning, TODO: stop pruning close to mate value
		fPrune = true;
	}
	else if (depthLeft == 2 && !inCheck && (evaluator.evaluatePosition(&board) + pieceValues[ROOK]) < alpha) {
		fPrune = true;
	}
	else if (depthLeft == 3 && !inCheck && (evaluator.evaluatePosition(&board) + pieceValues[QUEEN]) < alpha) {
		fPrune = true;
	}

	int numOfMoves = 0;
	bool raisedAlpha = false;
	int moves[218];
	int bestMoveIndex = 0;
	int newDepth = depthLeft - 1;
	int reductionDepth;
	TT_FLAG ttFlag = TT_ALPHA;
	bool legalMove = false;

	if (inCheck) {
		numOfMoves = board.getLegalMovesInCheck(moves);
		if (numOfMoves == 0) {
			return -(MATE_SCORE - ply);
		}
	}
	else numOfMoves = board.getLegalMoves(moves);

	sortMoves(moves, numOfMoves, ttMove, ply);

	if (isRepetition()) return contempt();
	
	for (int i = 0; i < numOfMoves; i++) {
		board.moveMake(moves[i]);
		if (board.inCheck(Color(board.sideToMove*(-1)))) { // move that puts its own king in check
			board.moveUnmake();
			continue;
		}
		reductionDepth = 0;
		newDepth = depthLeft - 1;
		legalMove = true;

		if (i >= 4 && !isPV && depthLeft >= 3 && !(moves[i] & MOVE_CAPTURE_MASK) && !(moves[i] & MOVE_PROMOTION_MASK) && !(board.inCheck(board.sideToMove))) { // LMR
			reductionDepth = 1;
			newDepth -= reductionDepth;
		}

		if (fPrune && legalMove && !(moves[i] & MOVE_CAPTURE_MASK) && !(moves[i] & MOVE_PROMOTION_MASK) && !(board.inCheck(board.sideToMove))) { // futility pruning
			board.moveUnmake();
			continue;
		}

		do { // research with full depth if reduced search actually improves over alpha.
			if (!raisedAlpha) { // principal variation search
				score = -alphaBeta(-beta, -alpha, newDepth, ply + 1, ALLOW_NULL, isPV);
			}
			else {
				if (-alphaBeta(-alpha - 1, -alpha, newDepth, ply + 1, ALLOW_NULL, NOT_PV) > alpha) {
					score = -alphaBeta(-beta, -alpha, newDepth, ply + 1, ALLOW_NULL, IS_PV);
				}
			}
		} while (score > alpha && reductionDepth && (newDepth += reductionDepth) && (reductionDepth = 0));
		
		board.moveUnmake();
		if (score > alpha) {
			if (score >= beta) {
				if (!(moves[i] & MOVE_CAPTURE_MASK)  && !(moves[i] & MOVE_PROMOTION_MASK)) {
					setKillers(moves[i], ply);
					historyMoves[moves[i] & MOVE_FROM_SQ_MASK][(moves[i] & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT] += depthLeft*depthLeft;
				}
				tTable.saveEntry(board.zobristKey, depthLeft, board.halfMoveCount - ply, beta, TT_BETA, moves[i]);
				return beta;
			}
			bestMoveIndex = i;
			ttFlag = TT_EXACT;
			raisedAlpha = true;
			alpha = score;
		}
	}
	if (timer.timesUp || stopSearch) {
		return alpha;
	}

	tTable.saveEntry(board.zobristKey, depthLeft, board.halfMoveCount - ply, alpha, ttFlag, moves[bestMoveIndex]);
	return alpha;
}

int Engine::quiescence(int alpha, int beta) {
	nodeCount++;
	int standPat = evaluator.evaluatePosition(&board);
	if (standPat >= beta) {
		return beta;
	}
	if (alpha < standPat) {
		alpha = standPat;
	}

	//TODO might have to check for in check here??
	int moves[218];
	int score = 0;
	int numOfCaptures = board.getQuiescenceMoves(moves);
	mvvLva(moves, numOfCaptures);
	for (int i = 0; i < numOfCaptures; i++) {
		if (!(board.phaseFlag & PHASE_EG) && !(moves[i] & MOVE_PROMOTION_MASK) && standPat + pieceValues[(moves[i] & MOVE_ATTACKED_PIECE_MASK) >> MOVE_ATTACKED_PIECE_SHIFT] + 200 < alpha) { // delta pruning
			continue;
		}
		if ((moves[i] & MOVE_CAPTURE_MASK) && SEE(((moves[i] & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT)) < 0) {
			continue;
		}
		board.moveMake(moves[i]);
		if (board.inCheck(Color(board.sideToMove*(-1)))) {
			board.moveUnmake();
			continue;
		}
		score = -quiescence(-beta, -alpha);

		board.moveUnmake();

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

	int moves[218];
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
			/*for (int j = depthLeft; j < maxDepth; j++) {
				std::cout << "\t";
			}
			std::cout << SQ_FILE((moves[i] & MOVE_FROM_SQ_MASK)) << SQ_RANK((moves[i] & MOVE_FROM_SQ_MASK)) << SQ_FILE(((moves[i] & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT)) << SQ_RANK(((moves[i] & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT)) << "\t";
			if (depthLeft != 1) {
				std::cout << nodes - nodesLast << std::endl;
			}
			else {
				std::cout << std::endl;
			}*/
				std::cout << SQ_FILE((moves[i] & MOVE_FROM_SQ_MASK)) << SQ_RANK((moves[i] & MOVE_FROM_SQ_MASK)) << SQ_FILE(((moves[i] & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT)) << SQ_RANK(((moves[i] & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT)) << "\t" << nodes - nodesLast << std::endl;
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

int Engine::miniMax(int depthLeft) {
	if (depthLeft == 0) return evaluator.evaluatePosition(&board);
	int score;
	int scoreMax = -20000;

	int moves[218];
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
		std::cout << "Move\tScore" << std::endl;
	}

	for (int i = 0; i < numOfMoves; i++) {

		board.moveMake(moves[i]);
		if (!board.inCheck(Color(board.sideToMove*(-1)))) {
			score = -miniMax(depthLeft - 1);
			for (int j = depthLeft; j < 3; j++) {
				std::cout << "\t";
			}
			std::cout << SQ_FILE(moves[i] & MOVE_FROM_SQ_MASK) << SQ_RANK(moves[i] & MOVE_FROM_SQ_MASK) << " " << SQ_FILE((moves[i] & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT) << SQ_RANK((moves[i] & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT) << "\t" << score << std::endl;
		}
		if (score > scoreMax) {
			scoreMax = score;
		}
		board.moveUnmake();
	}

	return scoreMax;
}

inline void Engine::mvvLva(int *moves, int numOfMoves) {
	std::sort(moves, moves + numOfMoves, [](int &lhs, int &rhs) {	return pieceSorting[(lhs & MOVE_ATTACKED_PIECE_MASK) >> MOVE_ATTACKED_PIECE_SHIFT] > pieceSorting[(rhs & MOVE_ATTACKED_PIECE_MASK) >> MOVE_ATTACKED_PIECE_SHIFT] ||
																		(pieceSorting[(lhs & MOVE_ATTACKED_PIECE_MASK) >> MOVE_ATTACKED_PIECE_SHIFT] == pieceSorting[(rhs & MOVE_ATTACKED_PIECE_MASK) >> MOVE_ATTACKED_PIECE_SHIFT] && pieceSorting[((lhs & MOVE_MOVED_PIECE_MASK) >> MOVE_ATTACKED_PIECE_SHIFT)] < pieceSorting[((rhs & MOVE_MOVED_PIECE_MASK) >> MOVE_ATTACKED_PIECE_SHIFT)]); });
}

int Engine::findBestMove(int *moves, int numOfMoves, int depth, int alpha, int beta, int *pvLines, int *scores) {
	int score = -20000;
	int bestIndex = 0;
	nodeCount++;
	int ttMove = -1;
	if (board.inCheck(board.sideToMove)) { // check extension
		depth++;
	}
	tTable.probe(board.zobristKey, depth, alpha, beta, &ttMove);
	sortMoves(moves, numOfMoves, ttMove, 0);
	for (int i = 0; i < numOfMoves; i++) {
		if (timer.timesUp || stopSearch) {
			pvLines[0] = EMPTY_MOVE;
			return INVALID;
		}
		board.moveMake(moves[i]);
		if (board.inCheck(Color(board.sideToMove*(-1)))) { // move that puts its own king in check
			board.moveUnmake();
			continue;
		}

		if ((i == 0) || ((score = -alphaBeta(-alpha - 1, -alpha, depth - 1, 1, ALLOW_NULL, NOT_PV)) > alpha)) { // principal variation search
			score = -alphaBeta(-beta, -alpha, depth - 1, 1, ALLOW_NULL, IS_PV);
		}

		board.moveUnmake();
		if (score > alpha) {
			alpha = score;
			bestIndex = i;
			if (score >= beta) {
				tTable.saveEntry(board.zobristKey, depth, board.halfMoveCount, beta, TT_BETA, moves[bestIndex]);
				pvLines[0] = moves[bestIndex];
				return beta;
			}
			tTable.saveEntry(board.zobristKey, depth, board.halfMoveCount, alpha, TT_ALPHA, moves[bestIndex]);
			infoPV(depth, score, false);
		}
		for (int k = 0; k < MULTI_PV_LINE_NUM; k++) {
			if (scores[k] == INVALID || score > scores[k]) {
				for (int j = MULTI_PV_LINE_NUM - 1; j > k; j--) {
					scores[j] = scores[j - 1];
					pvLines[j] = pvLines[j - 1];
				}
				pvLines[k] = moves[i];
				scores[k] = score;
				break;
			}
		}
	}

	tTable.saveEntry(board.zobristKey, depth, board.halfMoveCount, alpha, TT_EXACT, moves[bestIndex]);
	return alpha;
}

inline void Engine::sortMoves(int *moves, int numOfMoves, int bestMove, int ply) {
	for (int i = 0; i < numOfMoves; i++) {
		orderingValues[i] = 0;
		if (bestMove != EMPTY_MOVE && moves[i] == bestMove) {
			orderingValues[i] += HASH_ORDER;
			continue;
		}
		if (moves[i] & MOVE_CAPTURE_MASK || moves[i] & MOVE_PROMOTION_MASK) {
			if (moves[i] & MOVE_CAPTURE_MASK) {
				if (ply <= SEE_ORDER_DEPTH) {
					orderingValues[i] += SEE(((moves[i] & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT));
					orderingValues[i] += (orderingValues[i] > 0) ? CPT_ORDER : 0;
				}
				else {
					orderingValues[i] += (pieceValues[(moves[i] & MOVE_ATTACKED_PIECE_MASK) >> MOVE_ATTACKED_PIECE_SHIFT] - pieceValues[(moves[i] & MOVE_MOVED_PIECE_MASK) >> MOVE_MOVED_PIECE_SHIFT]) + CPT_ORDER;
				}
			}
			if (moves[i] & MOVE_PROMOTION_MASK) {
				if (((moves[i] & MOVE_PROMOTED_TO_MASK) >> MOVE_PROMOTED_TO_SHIFT) == QUEEN) {
					orderingValues[i] += (pieceValues[QUEEN] - pieceValues[PAWN]) + PROMOTION_ORDER;
				}
				else if (((moves[i] & MOVE_PROMOTED_TO_MASK) >> MOVE_PROMOTED_TO_SHIFT) == ROOK) {
					orderingValues[i] += (pieceValues[ROOK] - pieceValues[PAWN]) + PROMOTION_ORDER;
				}
				else if (((moves[i] & MOVE_PROMOTED_TO_MASK) >> MOVE_PROMOTED_TO_SHIFT) == BISHOP) {
					orderingValues[i] += (pieceValues[BISHOP] - pieceValues[PAWN]) + PROMOTION_ORDER;
				}
				else orderingValues[i] += (pieceValues[KNIGHT] - pieceValues[PAWN]) + PROMOTION_ORDER;
			}
		}
		else {
			if (moves[i] == killers[ply][KILLER_PRIMARY]) {
				orderingValues[i] += KILLER_ORDER;
			}
			else if (moves[i] == killers[ply][KILLER_SECONDARY]) {
				orderingValues[i] += KILLER_ORDER - 1;
			}
			else {
				orderingValues[i] += historyMoves[moves[i] & MOVE_FROM_SQ_MASK][(moves[i] & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT];
			}
		}
	}
	quicksort(moves, orderingValues, 0, numOfMoves - 1);
}

inline int Engine::partition(int *arr, int *order, const int left, const int right) {
	const int mid = left + (right - left) / 2;
	const int pivot = order[mid];
	// move the mid point value to the front.
	//std::swap(arr[mid], arr[left]);
	int temp = arr[left];
	arr[left] = arr[mid];
	arr[mid] = temp;
	temp = order[left];
	order[left] = order[mid];
	order[mid] = temp;

	int i = left + 1;
	int j = right;
	while (i <= j) {
		while (i <= j && order[i] >= pivot) {
			i++;
		}

		while (i <= j && order[j] < pivot) {
			j--;
		}

		if (i < j) {
			//std::swap(arr[i], arr[j]);
			temp = arr[j];
			arr[j] = arr[i];
			arr[i] = temp;
			temp = order[j];
			order[j] = order[i];
			order[i] = temp;

		}
	}
	//std::swap(arr[i - 1], arr[left]);
	temp = arr[left];
	arr[left] = arr[i - 1];
	arr[i - 1] = temp;
	temp = order[left];
	order[left] = order[i - 1];
	order[i - 1] = temp;

	return i - 1;
}

inline void Engine::quicksort(int *arr, int *order, const int left, const int right) {
	if (left >= right) {
		return;
	}

	int part = partition(arr, order, left, right);

	quicksort(arr, order, left, part - 1);
	quicksort(arr, order, part + 1, right);
}

int Engine::iterativeDeepening(int *moves, int numOfMoves, bool timed) {
	int score;
	Color findMoveFor = board.sideToMove;

	int pieceVal = board.getSideMaterialValue(WHITE) - pieceValues[KING];
	if (pieceVal <= 1200) {
		board.phaseFlag = PHASE_EG;
	}
	pieceVal = board.getSideMaterialValue(BLACK) - pieceValues[KING];
	if (pieceVal <= 1200) {
		board.phaseFlag = PHASE_EG;
	}
	
	int scores[MULTI_PV_LINE_NUM];
	int multiPVLines[MULTI_PV_LINE_NUM];
	int pvCount = 0;
	int windowMissCount = 0;
	int bestMove = 0;
	int alpha = -200000;
	int beta = 200000;
	int pvMove;
	unsigned long searchLength;
	stopSearch = false;
	nodeCount = 0;
	if (timed) {
		searchLength = calculateTimeForMove(findMoveFor);
	}
	else {
		searchLength = 0;
	}
	timer.start(true, searchLength);
	
	unsigned long searchStart;
	unsigned long long lastNodeCount = 0;
	int currDepth;
	decHistoryTable();

	if (mode == PROTO_NOTHING) {
		std::cout << std::left << std::setw(8) << std::setfill(' ') << "Depth";
		std::cout << std::left << std::setw(8) << std::setfill(' ') << "Misses";
		std::cout << std::left << std::setw(10) << std::setfill(' ') << "Nodes";
		std::cout << std::left << std::setw(10) << std::setfill(' ') << "Time(ms)";
		std::cout << std::left << std::setw(14) << std::setfill(' ') << "kNPS";
		std::cout << std::left << std::setw(8) << std::setfill(' ') << "Score";
		std::cout << std::left << std::setw(10) << std::setfill(' ') << "PV" << std::endl;
	}

	searchStart = timer.mseconds;

	for (currDepth = 1; currDepth <= maxDepth; currDepth++) {
		pvCount = 0;
		for (int i = 0; i < MULTI_PV_LINE_NUM; i++) {
			multiPVLines[i] = EMPTY_MOVE;
			scores[i] = INVALID;
		}
		score = findBestMove(moves, numOfMoves, currDepth, alpha, beta, multiPVLines, scores);
		if (multiPVLines[0] == EMPTY_MOVE) {
			//std::cout << "TIMES UP" << std::endl;
			break;
		}
		if (score <= alpha) {
			alpha -= pow(2 * WINDOW_SIZE, windowMissCount++);
			currDepth--;
			continue;
		}
		else if (score >= beta) {
			beta += pow(2 * WINDOW_SIZE, windowMissCount++);
			currDepth--;
			continue;
		}
		alpha = score - WINDOW_SIZE;
		beta = score + WINDOW_SIZE;
		for (pvCount = 1; pvCount < MULTI_PV_LINE_NUM; pvCount++) {
			if (multiPVLines[pvCount] == -1) {
				break;
			}
		}
		getSearchStats(currDepth, windowMissCount, lastNodeCount, searchStart, scores, multiPVLines, pvCount);
		bestMove = multiPVLines[0];
		if (wmsLeft != -1 && (timer.mseconds - searchStart) * DEPTH_TIME_INCREASE > searchLength - timer.mseconds) { // next depth would take longer than remaining time
			std::cout << "Ended search early as next depth would take " << (timer.mseconds - searchStart) * DEPTH_TIME_INCREASE << " ms and we have " << searchLength - timer.mseconds << " ms remaining" << std::endl;
			timer.stop();
			break;
		}
		if (numOfMoves == 1 && currDepth >= 4) { // only one legal reply, only search to depth 4 (nothing to decide, so why search it). 
			break;
		}
		lastNodeCount = nodeCount;
		windowMissCount = 0;
		searchStart = timer.mseconds;
	}
	//getSearchStats(-1, windowMissCount, 0, 0, INVALID, );

	return bestMove;
}

int Engine::SEE(int sq) { // TODO, can check for only legal moves, but this will slow down and might not be necessary as SEE is only used as a guideline
	int wAttackers[20];
	int bAttackers[20];

	int numOfwAttackers = board.getSqAttackers(wAttackers, sq, WHITE);
	int numOfbAttackers = board.getSqAttackers(bAttackers, sq, BLACK);

	std::sort(wAttackers, wAttackers + numOfwAttackers, [this](int lhs, int rhs) {return pieceSorting[board.board[lhs]] < pieceSorting[board.board[rhs]]; });
	std::sort(bAttackers, bAttackers + numOfbAttackers, [this](int lhs, int rhs) {return pieceSorting[board.board[lhs]] < pieceSorting[board.board[rhs]]; });

	int pieceAtSq = board.board[sq];
	int sideToMove = board.sideToMove;
	int score = 0;
	int newAttackerSq;
	bool inserted;

	int i = 0;

	while ((sideToMove == WHITE && i < numOfwAttackers) || (sideToMove == BLACK && i < numOfbAttackers)) {
		newAttackerSq = -1;
		if (sideToMove == WHITE && board.board[wAttackers[i]] == KING && numOfbAttackers - i > 0) {
			break; // piece is defended so king cant attack
		}
		else if (sideToMove == BLACK && board.board[bAttackers[i]] == KING && numOfwAttackers - i > 0) {
			break; 
		}
		score += pieceValues[pieceAtSq] * sideToMove * board.sideToMove;
		if (sideToMove == WHITE) {
			pieceAtSq = board.board[wAttackers[i]];
			if (board.board[wAttackers[i]] != KNIGHT) // no piece can be hiding behind a king or knight and still attack sq
				newAttackerSq = board.getSqOfFirstPieceOnRay(wAttackers[i], dirBySquareDiff[sq - wAttackers[i] + 119]);
		}
		else {
			pieceAtSq = board.board[bAttackers[i]];
			if (board.board[bAttackers[i]] != KNIGHT) // no piece can be hiding behind a king or knight and still attack sq
				newAttackerSq = board.getSqOfFirstPieceOnRay(bAttackers[i], dirBySquareDiff[sq - bAttackers[i] + 119]);
		}
		// find hidden attackers
		if (newAttackerSq != -1) {
			inserted = false;
			if (attackArray[sq - newAttackerSq + 128] & attackGroups[board.board[newAttackerSq]]) {
				if (board.boardColor[newAttackerSq] == WHITE) {
					for (int j = (sideToMove != board.sideToMove) ? i + 1 : i; j < numOfwAttackers; j++) {
						if (pieceSorting[board.board[newAttackerSq]] < pieceSorting[board.board[wAttackers[j]]]) {
							for (int k = numOfwAttackers; k > j; k--) {
								wAttackers[k] = wAttackers[k - 1];
							}
							wAttackers[j] = newAttackerSq;
							inserted = true;
							break;
						}
					}

					if (!inserted) { // the current highest value, therefore insert last
						wAttackers[numOfwAttackers] = newAttackerSq;
					}

					numOfwAttackers++;
				}
				else {
					for (int j = (sideToMove != board.sideToMove) ? i + 1 : i; j < numOfbAttackers; j++) {
						if (pieceSorting[board.board[newAttackerSq]] < pieceSorting[board.board[bAttackers[j]]]) {
							for (int k = numOfbAttackers; k > j; k--) {
								bAttackers[k] = bAttackers[k - 1];
							}
							bAttackers[j] = newAttackerSq;
							inserted = true;
							break;
						}
					}

					if (!inserted) { // the current highest value, therefore insert last
						bAttackers[numOfbAttackers] = newAttackerSq;
					}
					numOfbAttackers++;
				}
			}
		}

		sideToMove *= (-1);
		if (sideToMove == board.sideToMove) i++;
	}

	return score;
}

inline void Engine::getSearchStats(int searchDepth, int windoMissCount, unsigned long long prevNodeCount, unsigned long startTime, int* scores, int* pvLines, int pvCount) { // TODO, implement window misses
	const char separator = ' ';
	const int nameWidth = 8;
	const int numWidth = 10;

	switch (mode) {
		case PROTO_NOTHING:
			if (searchDepth == -1) {
				std::cout << "Total:\t";
			}
			else {
				std::cout << std::left << std::setw(nameWidth) << std::setfill(separator) << searchDepth;
				std::cout << std::left << std::setw(nameWidth) << std::setfill(separator) << windoMissCount;
			}
			//std::cout << "Search to depth " << searchDepth << " took " << timer.mseconds - startTime << " milliseconds, and searched " << nodeCount - prevNodeCount << " nodes" << std::endl;
			std::cout << std::left << std::setw(numWidth) << std::setfill(separator) << nodeCount - prevNodeCount;
			std::cout << std::left << std::setw(numWidth) << std::setfill(separator) << timer.mseconds - startTime;
			infoNPS(nodeCount - prevNodeCount, startTime);
			if (searchDepth != -1) {
				std::cout << "\t";
				for (int i = 0; i < pvCount; i++) {
					if (i != 0) {
						std::cout << "\t\t\t\t\t\t(";
					}
					infoPV(searchDepth, scores[i], true, pvLines[i]);
					if (i != 0) {
						std::cout << ")";
					}
					std::cout << std::endl;
				}
				
			}
			break;
		case PROTO_UCI:
			infoNPS(nodeCount - prevNodeCount, startTime);
			if (searchDepth != -1)
				infoPV(searchDepth, scores[0], true);
			break;
	}
	
}

inline unsigned long Engine::calculateTimeForMove(Color side) {
	if (mode == PROTO_UCI) {
			int timeLeft = (side == WHITE) ? wmsLeft : bmsLeft;
			int timeInc = (side == WHITE) ? wTimeInc : bTimeInc;
			if (timeLeft == -1) { //not timed game
				return FIXED_SEARCH_DURATION;
			}
			float fractionAllowed = (timeInc > 0) ? 0.08 : 0.04;
			if (board.halfMoveCount < 20) {
				fractionAllowed = fractionAllowed / 2;
			} // TODO, something with 0 increment
			std::stringstream ss;
			ss << "Gonna use " << timeLeft * fractionAllowed + 0.8*timeInc << " mseconds";
			comSend(ss.str());
			return timeLeft * fractionAllowed + timeInc;
			
	}
	else if (mode == PROTO_NOTHING) {
		return FIXED_SEARCH_DURATION;
	}
	
}

inline void Engine::infoNPS(unsigned long long nodes, unsigned long startTime) {
	unsigned long elapsedTime = timer.mseconds - startTime;
	if (elapsedTime) {
		switch (mode) {
			case PROTO_NOTHING:
				std::cout << std::left << std::setw(6) << std::setfill(' ') << (nodes / double((timer.mseconds - startTime)));
				break;
			case PROTO_UCI:
				std::stringstream ss;
				ss << "info nps " << (nodes / double((timer.mseconds - startTime))) * 1000;
				comSend(ss.str());
				break;
		}
	}
	else if (mode == PROTO_NOTHING) {
		std::cout << std::left << std::setw(7) << std::setfill(' ') << "N/A";
	}
}

void Engine::infoPV(int searchDepth, int score, bool depthFinished, int pvLine) {
	if (!depthFinished && mode == PROTO_NOTHING) {
		return;
	}
	int pvLength = 0;
	TT_FLAG lastMoveFlag;
	int pvMove = pvLine;
	std::string pvString;
	std::stringstream pvSS;
	switch (mode) {
		case PROTO_UCI:
			pvSS << "info depth " << searchDepth << " pv ";
			break;
	}
	int j;
	for (j = 0; j < searchDepth; j++) {
		if ((lastMoveFlag = tTable.getPV(board.zobristKey, &pvMove)) == TT_INVALID && j != 0) {
			break;
		}
		pvSS << SQ_FILE((pvMove & MOVE_FROM_SQ_MASK)) << SQ_RANK((pvMove & MOVE_FROM_SQ_MASK)) << SQ_FILE(((pvMove & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT)) << SQ_RANK(((pvMove & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT)) << " ";
		board.moveMake(pvMove);
		pvLength++;
	}
	if (score == INVALID) {
		if (j == searchDepth) {
			switch (mode) {
			case PROTO_NOTHING:
				pvString = std::to_string(evaluator.evaluatePosition(&board)*board.sideToMove) + "\t" + pvSS.str();
				break;
			case PROTO_UCI:
				pvSS << "score cp " << evaluator.evaluatePosition(&board)*board.sideToMove;
				break;
			}
		}
		else if (mode == PROTO_NOTHING) {
			pvString = "N/A\t" + pvSS.str();
		}
		
	}
	else if (mode == PROTO_UCI) {
		pvSS << "score cp " << score;
	}
	else if (mode == PROTO_NOTHING) {
		pvString = std::to_string(score) + "\t" + pvSS.str();
	}

	for (int j = 0; j < pvLength; j++) {
		board.moveUnmake();
	}

	switch (mode) {
		case PROTO_NOTHING:
			std::cout << pvString;
			break;
		case PROTO_UCI:
			comSend(pvSS.str());
	}
}


inline void Engine::decHistoryTable() {
	for (int i = 0; i < 120; i++) {
		for (int j = 0; j < 120; j++) {
			historyMoves[i][j] = historyMoves[i][j] / 8;
		}
	}
}

inline void Engine::setKillers(int move, int ply) {
	if (move != killers[ply][KILLER_PRIMARY]) {
		killers[ply][KILLER_SECONDARY] = killers[ply][KILLER_PRIMARY];
		killers[ply][KILLER_PRIMARY] = move;
	}
}

void Engine::playGame(std::string fen) {
	if (fen != "") {
		board.loadFromFen(fen);
	}

	int moves[218];
	int numOfMoves;
	int move;

	board.printBoard();

	while (1) {
		if (board.inCheck(board.sideToMove)) {
			numOfMoves = board.getLegalMovesInCheck(moves);
			if (numOfMoves == 0) {
				break;
			}
		}
		else {
			numOfMoves = board.getLegalMoves(moves);
		}
		move = iterativeDeepening(moves, numOfMoves, true);
		board.moveMake(move);
		board.printBoard();
		board.printMove(move);
	}

	std::cout << ((board.sideToMove == WHITE) ? "Black" : "White") << " won in " << board.halfMoveCount / 2 << " moves!" << std::endl;
}



void Engine::comInit() {
	unsigned long dw;
	hstdin = GetStdHandle(STD_INPUT_HANDLE);
	pipe = !GetConsoleMode(hstdin, &dw);
	if (!pipe) {
		SetConsoleMode(hstdin, dw&~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
		FlushConsoleInputBuffer(hstdin);
		mode = PROTO_NOTHING;
		comSend("Type help for commands");
	}
	else {
		std::cout.rdbuf()->pubsetbuf(NULL, 0);
		std::cin.rdbuf()->pubsetbuf(NULL, 0);
		mode = PROTO_UCI; 
	}

	task = TASK_NOTHING;
	
}

int Engine::comReceive() {
	std::string command;

	if (!comInput()) return 0;


	while (1) {
		std::getline(std::cin, command);
		std::thread serviceCommandThread;

		switch (mode) {
			case PROTO_NOTHING: 
				serviceCommandThread = std::thread([this, command] {this->comNothing(command); });
				serviceCommandThread.detach();
				break;
			case PROTO_UCI:
				serviceCommandThread = std::thread([this, command] {this->comUCI(command); });
				serviceCommandThread.detach();
				break;
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

int Engine::comNothing(std::string command) {
	std::string buf; // Have a buffer string
	std::stringstream ss(command); // Insert the string into a stream

	std::vector<std::string> tokens; // Create vector to hold our words

	while (ss >> buf)
		tokens.push_back(buf);

	if (tokens[0] == "help") {
		comSend("print board\tPrint board");
		comSend("move xxxx\tMake move");
		comSend("print moves\tPrint moves");
		comSend("go\t\tFind move");
		comSend("fen [fen]\tGet current fen or load from given fen");
		comSend("stop\t\tStop current search");
		comSend("analyze\t\tAnalyze position");
		comSend("evaluate\t\tEvaluate position");
		comSend("exit\t\tEnd process");
	}
	else if (tokens[0] == "exit") {
		exit(0);
	}
	else if (tokens[0] == "stop") {
		stopSearch = true;
	}
	else if (tokens[0] == "fen") {
		if (tokens.size() > 1) {
			board.loadFromFen(tokens[1]);
		}
		else comSend(board.getFenString());
	}
	else if (tokens[0] == "evaluate") {
		comSend(std::to_string(evaluator.evaluatePosition(&board) * board.sideToMove) + ", higher is better for white");
	}
	else if (tokens[0] == "go") {
		bool timed = (tokens.size() > 1 && tokens[1] == "infinite") ? false : true;
		board.printBoard();
		int moves[218];
		int numOfMoves;
		int move;
		std::stringstream moveStream;
		if (board.inCheck(board.sideToMove)) {
			numOfMoves = board.getLegalMovesInCheck(moves);
		}
		else {
			numOfMoves = board.getLegalMoves(moves);
		}
		move = iterativeDeepening(moves, numOfMoves, timed);
		moveStream << "move found " << SQ_FILE((move & MOVE_FROM_SQ_MASK)) << SQ_RANK((move & MOVE_FROM_SQ_MASK)) << SQ_FILE(((move & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT)) << SQ_RANK(((move & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT));
		if (move & MOVE_PROMOTION_MASK) {
			switch ((move & MOVE_PROMOTED_TO_MASK) >> MOVE_PROMOTED_TO_SHIFT) {
			case QUEEN:
				moveStream << "q";
				break;
			case ROOK:
				moveStream << "r";
				break;
			case BISHOP:
				moveStream << "b";
				break;
			case KNIGHT:
				moveStream << "n";
				break;
			}
		}
		comSend(moveStream.str());
		board.moveMake(move);

		board.printBoard();
	}
	else if (tokens[0] == "print" && tokens.size() > 1) {
		if (tokens.size() > 1 && tokens[1] == "moves") {
			int moves[218];
			int numOfMoves;
			if (board.inCheck(board.sideToMove)) numOfMoves = board.getLegalMovesInCheck(moves);
			else numOfMoves = board.getLegalMoves(moves);

			board.printMoves(moves, numOfMoves);
		} else if (tokens.size() > 1 && tokens[1] == "board") 
			board.printBoard();
	}
	else if (tokens[0] == "play") {
		if (tokens.size() == 2) // if fen input
			playGame(tokens[1]);
		else
			playGame();
	}
	else if (tokens[0] == "move") {
		int move = 0;
		std::string moveString = tokens.back();
		std::string moveFrom = moveString.substr(0, 2);
		std::string moveTo = moveString.substr(2);

		int sqFrom = SQ_STR_TO_INT(moveFrom);
		int sqTo = SQ_STR_TO_INT(moveTo.substr(0, 2));

		if (sqFrom == (board.history[board.historyIndex].move & MOVE_FROM_SQ_MASK) && sqTo == ((board.history[board.historyIndex].move & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT)) {
			comSend("ignoring my own move");
			return 0; // the move the engine did so we ignore it
		}

		move = sqFrom | (sqTo << MOVE_TO_SQ_SHIFT);
		move |= (board.board[sqFrom]) << MOVE_MOVED_PIECE_SHIFT;

		if (moveTo.length() == 3) { //promotion
			move |= (1 << MOVE_PROMOTION_SHIFT);
			switch (moveTo.back()) {
			case 'q':
				move |= (QUEEN << MOVE_PROMOTED_TO_SHIFT);
				break;
			case 'r':
				move |= (ROOK << MOVE_PROMOTED_TO_SHIFT);
				break;
			case 'b':
				move |= (BISHOP << MOVE_PROMOTED_TO_SHIFT);
				break;
			case 'k':
				move |= (KNIGHT << MOVE_PROMOTED_TO_SHIFT);
				break;
			default:
				comSend("Unexpected third parameter in moveTo, was:");
				comSend(moveTo);
				break;
			}
		}

		if (board.board[sqTo] != EMPTY)
			move |= (1 << MOVE_CAPTURE_SHIFT);
		move |= (board.board[sqTo] << MOVE_ATTACKED_PIECE_SHIFT);
		if (sqTo == board.enPassant) move |= (1 << MOVE_EN_PASSANT_SHIFT);

		if (board.board[sqFrom] == KING && abs(sqFrom - sqTo) == 2) {
			if (sqFrom > sqTo) {
				move |= (1 << MOVE_CASTLE_LONG_SHIFT);
			}
			else {
				move |= (1 << MOVE_CASTLE_SHORT_SHIFT);
			}
		}
		bool moveIsLegal = false;

		int moves[218];
		int numOfMoves;
		if (board.inCheck(board.sideToMove)) numOfMoves = board.getLegalMovesInCheck(moves);
		else numOfMoves = board.getLegalMoves(moves);
		for (int i = 0; i < numOfMoves; i++) {
			if (moves[i] == move) {
				moveIsLegal = true;
				break;
			}
		}
		if (moveIsLegal) {
			board.moveMake(move);
			board.printBoard();
		}
		else {
			comSend("move is not legal, use 'print moves' to print all legal moves");
		}
	}
	else {
		comSend("command not recognized, type help for available commands");
	}
	
	return 1;
}

int Engine::moveStringToInt(std::string moveString) {
	std::string moveFrom = moveString.substr(0, 2);
	std::string moveTo = moveString.substr(2);
	int move = 0;

	int sqFrom = SQ_STR_TO_INT(moveFrom);
	int sqTo = SQ_STR_TO_INT(moveTo.substr(0, 2));

	if (sqFrom == (board.history[board.historyIndex].move & MOVE_FROM_SQ_MASK) && sqTo == ((board.history[board.historyIndex].move & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT)) {
		comSend("ignoring my own move");
		return 0; // the move the engine did so we ignore it
	}

	move = sqFrom | (sqTo << MOVE_TO_SQ_SHIFT);
	move |= (board.board[sqFrom]) << MOVE_MOVED_PIECE_SHIFT;

	if (moveTo.length() == 3) { //promotion
		move |= (1 << MOVE_PROMOTION_SHIFT);
		switch (moveTo.back()) {
		case 'q':
			move |= (QUEEN << MOVE_PROMOTED_TO_SHIFT);
			break;
		case 'r':
			move |= (ROOK << MOVE_PROMOTED_TO_SHIFT);
			break;
		case 'b':
			move |= (BISHOP << MOVE_PROMOTED_TO_SHIFT);
			break;
		case 'k':
			move |= (KNIGHT << MOVE_PROMOTED_TO_SHIFT);
			break;
		default:
			comSend("Unexpected third parameter in moveTo, was:");
			comSend(moveTo);
			break;
		}
	}

	if (board.board[sqTo] != EMPTY)
		move |= (1 << MOVE_CAPTURE_SHIFT);
	move |= (board.board[sqTo] << MOVE_ATTACKED_PIECE_SHIFT);
	if (sqTo == board.enPassant) move |= (1 << MOVE_EN_PASSANT_SHIFT);

	if (board.board[sqFrom] == KING && abs(sqFrom - sqTo) == 2) {
		if (sqFrom > sqTo) {
			move |= (1 << MOVE_CASTLE_LONG_SHIFT);
		}
		else {
			move |= (1 << MOVE_CASTLE_SHORT_SHIFT);
		}
	}

	return move;
}

int Engine::comUCI(std::string command) {
	if (command == "uci") {
		comSend("id name c++hess Bugged");
		comSend("id author Eivindeb");
		comSend("uciok");
	}
	else if (command == "isready") {
		comSend("readyok");
	}
	else if (command == "quit") {
		comSend("exiting");
		exit(0);
	}
	else if (command == "stop") {
		stopSearch = true;
	}
	else if (command == "ucinewgame") {
		board = Board();
		tTable.clearTables();
	}
	else {
		std::string buf; // Have a buffer string
		std::stringstream ss(command); // Insert the string into a stream

		std::vector<std::string> tokens; // Create vector to hold our words

		WaitForSingleObject(boardLock, INFINITE); //aquire board lock

		while (ss >> buf)
			tokens.push_back(buf);

		if (tokens[0] == "position") {
			if (tokens[1] == "startpos" && tokens.size() == 2) {
				board.loadFromFen(START_FEN);
			}
			else if (board.historyIndex == -1) {
				int moveStartIndex;
				comSend("Loading from pgn");
				if (tokens[1] == "fen") {
					std::string fen = tokens[2] + " " + tokens[3] + " " + tokens[4] + " " + tokens[5] + " " + tokens[6] + " " + tokens[7];
					board.loadFromFen(fen);
					comSend("Fen " + fen);
					moveStartIndex = 9;
				}
				else {
					board.loadFromFen(START_FEN);
					moveStartIndex = 3;
				}
				for (int i = moveStartIndex; i < tokens.size(); i++) {
					board.moveMake(moveStringToInt(tokens[i]));
				}
			}
			else {
				int move = moveStringToInt(tokens.back());
				board.printMove(move);
				board.moveMake(move);
			}
		}
		else if (tokens[0] == "go") {
			bool timed = true;
			if (tokens.size() > 1) {
				for (int i = 1; i < tokens.size(); i++) {
					if (tokens[i] == "infinite") {
						wmsLeft = -1;
						bmsLeft = -1;
						timed = false;
						break;
					}
					if (tokens[i] == "wtime") {
						wmsLeft = std::stoi(tokens[++i]);
					}
					else if (tokens[i] == "btime") {
						bmsLeft = std::stoi(tokens[++i]);
					}
					else if (tokens[i] == "winc") {
						wTimeInc = std::stoi(tokens[++i]);
					}
					else if (tokens[i] == "binc") {
						bTimeInc = std::stoi(tokens[++i]);
					}
				}
			}
			int moves[218];
			int numOfMoves;
			int move;
			std::stringstream moveStream;
			if (board.inCheck(board.sideToMove)) {
				numOfMoves = board.getLegalMovesInCheck(moves);
			}
			else {
				numOfMoves = board.getLegalMoves(moves);
			}
			move = iterativeDeepening(moves, numOfMoves, timed);

			moveStream << "bestmove " << SQ_FILE((move & MOVE_FROM_SQ_MASK)) << SQ_RANK((move & MOVE_FROM_SQ_MASK)) << SQ_FILE(((move & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT)) << SQ_RANK(((move & MOVE_TO_SQ_MASK) >> MOVE_TO_SQ_SHIFT));
			if (move & MOVE_PROMOTION_MASK) {
				switch ((move & MOVE_PROMOTED_TO_MASK) >> MOVE_PROMOTED_TO_SHIFT) {
					case QUEEN:
						moveStream << "q";
						break;
					case ROOK:
						moveStream << "r";
						break;
					case BISHOP:
						moveStream << "b";
						break;
					case KNIGHT:
						moveStream << "n";
						break;
				}
			}
			comSend(moveStream.str());
			board.moveMake(move);
		}
		ReleaseMutex(boardLock);
	}

	return 0;
}

bool Engine::isRepetition() {
	for (int i = 0; i < board.repIndex; i++) {
		if (board.repStack[i] == board.zobristKey) {
			return true;
		}
	}
	return false;
}

int Engine::contempt() {
	int score = DRAW_OPENING;

	if (board.getSideMaterialValue(findMoveFor) < ENDGAME_MAT) score = DRAW_ENDGAME;

	return score * findMoveFor;
}

void Engine::comSend(std::string command) {
	std::cout << command << std::endl;
}