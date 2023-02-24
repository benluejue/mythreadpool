#ifndef THREAD_POOL
#define THREAD_POOL
#include <vector>
#include <memory>
#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
class Task {
	virtual void run() = 0;
};
enum class PoolMode {
	FIXED,
	CACHED
};
class Thread {
public:
	//绑定的 这个函数void ThreadPool​::threadFunc();
	using ThreadFunc = std::function<void()>;
	//开启线程
	void start();
	//线程构造
	Thread(ThreadFunc func);
	//线程析构
	~Thread();
private:
	ThreadFunc func_;
};
class ThreadPool​
{
public:
	ThreadPool​();
	~ThreadPool​();

	//开启线程池
	void start(int initTHreadSize);
	//设置模式
	void setMode(PoolMode mode);
	// 设置 任务队列的最大上限
	void setTaskQueMaxThreadHould(int threshould);
	//设置 初始线程数量
	void setInitThreadSize(int size);
	// 提交任务 用户调用该接口 传入任务对象 生成任务
	// shared_ptr 与 unique_ptr的区别为 unique在某一时刻只能指向一个对象
	void submitTask(std::shared_ptr<Task> sp);
	// 线程处理函数 线程池的所有线程从 任务队列里 消费任务
	void threadFunc();

private:
	// 线程列表
	std::vector<std::unique_ptr<Thread>>threads_;
	// 初始线程数量
	int initThreadsSize_;
	// 线程队列
	std::queue<std::shared_ptr<Thread>>taskQue_;
	// 任务的数量
	std::atomic_int taskSize_;
	// 任务的数量上限
	size_t taskQueMaxThread_;
	// 保障任务队列线程安全的锁
	std::mutex taskQueMtx_;
	// 条件变量 配合std::unqiue_lock<std::mutex> 使用
	// 当某个线程 被 wait调用，unqiue_lock锁住当前线程，直到另外一个线程调用notification 
	std::condition_variable notFull_, notEmpty_;
	PoolMode poolMode_;

};


#endif // !THREAD_POOL

