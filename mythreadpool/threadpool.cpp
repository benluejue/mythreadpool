#include "threadpool.h"
#include <iostream>
const int TASK_MAX_THRESHHOLD = 1024;

ThreadPool​::ThreadPool​()
	:initThreadsSize_(4),
	taskSize_(0),
	taskQueMaxThread_(TASK_MAX_THRESHHOLD),
	poolMode_(PoolMode::FIXED)
{
}

ThreadPool​::~ThreadPool​()
{
}

void ThreadPool​::setMode(PoolMode mode)
{
	this->poolMode_ = mode;
}
void ThreadPool​::setTaskQueMaxThreadHould(int threshould)
{
	this->taskQueMaxThread_ = threshould;
}
void ThreadPool​::setInitThreadSize(int size)
{
	this->initThreadsSize_ = size;
}
void ThreadPool​::submitTask(std::shared_ptr<Task> sp)
{
	//	获取锁 https://stackoverflow.com/questions/40321587/what-all-should-i-know-about-stdunique-lock
	//  std::unique_lock对象独占所有权(unique owership)，对mutex进行上锁，解锁操作。
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	// 线程的通讯，等待任务队伍有空余 如果阻塞了，一定会把lock释放掉
	// blocks the current thread until the condition variable is awakened
	/*while (taskQue_.size() ==taskQueMaxThread_) {
		notFull_.wait(lock);
	}*/
	// notFull_.wait(lock, [&]()->bool {return taskQue_.size() < taskQueMaxThread_; });
	// 增加需求，wait 1秒
	if ( !notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool {return taskQue_.size() < taskQueMaxThread_; })) {
		// 如果返回false 说明在这1s内没有满足条件
		return;
	}
	// 如果有空余 把 任务放入 任务队列中
	taskQue_.emplace(sp);
	// 为什么可以直接 使用taskQue_.size 查看任务数量，还要让这个++呢？
	taskSize_++;

	// 因为放了新任务，任务队列肯定不为空了，
	// 常用来唤醒阻塞的线程notEmpty notifies all waiting threads
	notEmpty_.notify_all();
}
// 线程处理函数 线程池的所有线程从 任务队列里 消费任务
void ThreadPool​::threadFunc()
{
	/*std::cout << "begin threadFunc tid: "<< 
		std::this_thread::get_id() << std::endl;

	std::cout << "end threadFunc tid: " <<
		std::this_thread::get_id() << std::endl;*/
	// 先获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	// 等待notempty条件
	notEmpty_.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return taskQue_.size() > 0; });
	// 从任务队列里取出一个任务
	
	// 当前线程负责处理这个任务

}
void ThreadPool​::start(int initTHreadSize=3)
{
	this->initThreadsSize_ = initTHreadSize;
	// 创建线程对象
	for (int i = 0; i < initTHreadSize; i++) {
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool​::threadFunc, this));
		//std::vector<std::unique_ptr<Thread>>   threads_;
		threads_.emplace_back(std::move(ptr));
	}
	// 启动线程 std::vector<Thread*>threads_;
	for (int i = 0; i < initTHreadSize; i++) {
		threads_[i]->start();
	}
}

Thread::Thread(ThreadFunc func)
	:func_(func)
{
}

Thread::~Thread()
{
}
void Thread::start()
{
	//创建一个线程来执行 一个线程函数
	std::thread t(func_);
	t.detach();  // 设置线程分离
}

