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
		int wmsLeft;
		int wTimeInc;
		int bmsLeft;
		int bTimeInc;
	private:
		bool timerStop;
		std::thread::id threadId;
		unsigned long msecondsMax;
};