#include <iostream>
#include <chrono>
#include <thread>
#include "threadpool.h"

using namespace std;
int main() {
	ThreadPool​ testPool;
	testPool.start(5);
	std::this_thread::sleep_for(std::chrono::seconds(5));

	return 0;
}