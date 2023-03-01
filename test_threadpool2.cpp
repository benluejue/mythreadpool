#include <iostream>
#include <functional>
#include <thread>
#include <future>
#include "threadpool.h"
using namespace std;
const int numThreads = 4;
int sum1(int a, int b) {
	return a + b;
}
int sum2(int a, int b, int c) {
	return a + b + c;
}
int main() {
	ThreadPool pool;
	pool.setMode(PoolMode::CACHED);
	pool.start(numThreads);

	//packaged_task<int(int, int)>task(sum1);
	//future<int>res = task.get_future();
	/*cout << res.get();*/
	future<int> res1 =  pool.submitTask(sum1, 1, 2);
	future<int> res2 = pool.submitTask(
		[](int a, int b)->int {
			int res = 0;
			for (int i = a; i <= b; i++) res += i;
			return res;
		},1,20
	);
	future<int> res3 = pool.submitTask(sum2, 1, 2,3);
	// get语句是会阻塞的，如果任务还没有执行完就会阻塞，等待结果
	cout << res1.get()<<endl;
	cout << res2.get()<<endl;
	cout << res3.get()<<endl;

	return 0;
}
