#pragma once
#include <thread>


class Timer {
	public:
		Timer(int max);
		void start(bool fromStart);
		void incTimer();
		unsigned long mseconds;
		int secondsMax;
		bool timesUp;
	private:
		std::thread::id threadId;
};