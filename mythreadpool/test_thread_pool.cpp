#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include "threadpool.h"

using namespace std;
using uLong = unsigned long long;


class my_task :public Task
{
public:
	my_task(int begin, int end)
		:begin_(begin), end_(end)
	{}
public:
	Any run() {
		std::cout << "begin tid: "<<
			std::this_thread::get_id()<<" beigin! " << std::endl;

		uLong sum = 0;
		//std::this_thread::sleep_for(std::chrono::seconds(3));
		for (uLong i = begin_; i <= end_; i++) sum += i;
		std::cout << "begin tid: " <<
			std::this_thread::get_id() << " end! " << std::endl;
		return sum;
	}
	// 计算从begin到end的和
private:
	int begin_;
	int end_;
};



int main() {
#if  1
	{
		ThreadPool testPool;
		testPool.setMode(PoolMode::CACHED);
		testPool.start(4);

		Result res1 = testPool.submitTask(std::make_shared<my_task>(1, 100000000));
		testPool.submitTask(std::make_shared<my_task>(1, 100000000));
		testPool.submitTask(std::make_shared<my_task>(1, 100000000));
		testPool.submitTask(std::make_shared<my_task>(1, 100000000));
		testPool.submitTask(std::make_shared<my_task>(1, 100000000));
		testPool.submitTask(std::make_shared<my_task>(1, 100000000));
		testPool.submitTask(std::make_shared<my_task>(1, 100000000));
		testPool.submitTask(std::make_shared<my_task>(1, 100000000));
		uLong sum1 = res1.get().cast_<uLong>();
		cout << sum1 << endl;
		
	}
	cout << "main thread over" << endl;
#endif //  0

	
#if 0
	{
		ThreadPool testPool;
		// 用户设置线程模式
		testPool.setMode(PoolMode::CACHED);

		testPool.start(4);
		// 将小任务 分解为多个认为
		Result res1 = testPool.submitTask(std::make_shared<my_task>(0, 100000000));
		Result res2 = testPool.submitTask(std::make_shared<my_task>(100000001, 200000000));
		Result res3 = testPool.submitTask(std::make_shared<my_task>(200000001, 300000000));

		testPool.submitTask(std::make_shared<my_task>(200000001, 300000000));
		testPool.submitTask(std::make_shared<my_task>(200000001, 300000000));
		testPool.submitTask(std::make_shared<my_task>(200000001, 300000000));

		uLong sum1 = res1.get().cast_<uLong>();
		uLong sum2 = res2.get().cast_<uLong>();
		uLong sum3 = res3.get().cast_<uLong>();
		cout << sum1 + sum2 + sum3 << endl;

	}
#endif
	
	auto a = getchar();

	return 0;
}

