#include "stdafx.h"
#include "Timer.h"
#include <chrono>
#include <thread>

Timer::Timer(int max) {
	secondsMax = max;
}

void Timer::start(bool fromStart) {
	timesUp = false;
	if (fromStart == true) {
		mseconds = 0;
	}

	std::thread t1 = std::thread([this] {this->incTimer(); });
	threadId = t1.get_id();
	t1.detach();
}

void Timer::incTimer() {
	while (mseconds < secondsMax * 1000) {
		auto start = std::chrono::high_resolution_clock::now();
		std::this_thread::sleep_for(std::chrono::seconds(1));
		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end - start;
		mseconds += elapsed.count();
	}
	timesUp = true;
	return;
}