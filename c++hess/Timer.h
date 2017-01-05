#pragma once
#include <thread>


class Timer {
	public:
		Timer();
		void start(bool fromStart, unsigned long duration);
		void incTimer();
		unsigned long mseconds;
		
		bool timesUp;
	private:
		std::thread::id threadId;
		unsigned long msecondsMax;
};