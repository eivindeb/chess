#include "stdafx.h"
#include "Timer.h"
#include <chrono>
#include <thread>

Timer::Timer() {
	msecondsMax = 10*1000;
}

void Timer::start(bool fromStart, unsigned long duration) {
	timesUp = false;
	if (fromStart == true) {
		mseconds = 0;
	}
	msecondsMax = duration;

	std::thread t1 = std::thread([this] {this->incTimer(); });
	threadId = t1.get_id();
	t1.detach();
}

void Timer::incTimer() {
	while (mseconds < msecondsMax) {
		auto start = std::chrono::high_resolution_clock::now();
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end - start;
		mseconds += elapsed.count();
	}
	timesUp = true;
	return;
}