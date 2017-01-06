#pragma once
#include <thread>


class Timer {
	public:
		Timer();
		void start(bool fromStart, unsigned long duration);
		void stop();
		void incTimer();
		unsigned long mseconds;
		bool timesUp;
	private:
		bool timerStop;
		std::thread::id threadId;
		unsigned long msecondsMax;
};