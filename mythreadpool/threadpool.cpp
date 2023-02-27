#include "threadpool.h"
#include <iostream>
const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THREADHILD = 10;
const int THREAD_MAX_IDLE_TIME = 10;

ThreadPool::ThreadPool()
	: initThreadsSize_(0)
	, taskSize_(0)
	, idleThreadSize_(0)
	, taskQueMaxThread_(TASK_MAX_THRESHHOLD)
	, threadSizeThresHold_(THREAD_MAX_THREADHILD)
	, poolMode_(PoolMode::FIXED)
	, isPoolRunning_(false)
{
}

// 析构
ThreadPool::~ThreadPool()
{
	// 等待所有线程结束
	// 两种状态，1阻塞，2 正在进行任务
	isPoolRunning_ = false;
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0;});

}

void ThreadPool::setMode(PoolMode mode)
{
	// 运行中不允许设置模式
	if (checkRunningState())
		return;
	poolMode_ = mode;
}
void ThreadPool::setTaskQueMaxThreadHould(int threshould)
{
	this->taskQueMaxThread_ = threshould;
}
void ThreadPool::setInitThreadSize(int size)
{
	this->initThreadsSize_ = size;
}
void ThreadPool::setThreadSizeThreadHould(int threadhold)
{
	this->threadSizeThresHold_ = threadhold;
}
// 生产者
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
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
	if ( !notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThread_; })) {
		// 如果返回false 说明在这1s 
		std::cout << "任务提交失败!!!!!" << std::endl;
		return Result(sp, false);
	}
	// 如果有空余 把 任务 放入 任务队列中
	taskQue_.emplace(sp);
	// 为什么可以直接 使用taskQue_.size 查看任务数量，还要让这个++呢？
	taskSize_++;
	// 因为放了新任务，任务队列肯定不为空了，
	// 常用来唤醒阻塞的线程notEmpty notifies all waiting threads
	notEmpty_.notify_all();

	// 在cached模式下，根据线程的数量 和 闲置线程的数量 创建新的线程
	if (this->poolMode_ == PoolMode::CACHED
		&& taskSize_ > idleThreadSize_ 
		&& curThreadSize_ < threadSizeThresHold_) 
	{
		// idle 无所事事
		std::cout << ">>> creat new thread...\n";
		// 创建新线程						 = new std::bind()
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		// 每创建一个线程，开启线程，线程开始等待执行任务
		threads_[threadId]->start();
		curThreadSize_++;
		idleThreadSize_++;
	}

	return Result(sp);
}
// 线程处理函数 线程池的所有线程从 任务队列里 消费任务
void ThreadPool::threadFunc(int threadid)
{
	// resolution 分辨率
	auto lastTime = std::chrono::high_resolution_clock().now();
	for(;;)
	{
		std::shared_ptr<Task> cur_task;
		{	
			// 先获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			std::cout << "tid: " <<std::this_thread::get_id() 
				<<"尝试获取任务！" << std::endl;
			// todo在cachaed模式下，我们肯能创建了很多线程，需要把闲置时间多余60s的删掉
			// 当前时间-上一次执行的时间 > 60
			// 等待notempty条件
			// 锁加双重判断 
			while (taskQue_.size() == 0) {
				if (!isPoolRunning_)
				{
					threads_.erase(threadid);
					std::cout << "thread id " << std::this_thread::get_id() << " exit,flag2" << std::endl;
					exitCond_.notify_all();
					return;
				}
				if (ThreadPool::poolMode_ == PoolMode::CACHED) {
					// 一秒一返回
					// wait_for 返回值cv_status，枚举类型有两种返回值  
					// timeout no_timeout分别表示 条件变量 是否为 超时返回
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1))) {
						auto curTime = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(curTime - lastTime);
						if (dur.count() > THREAD_MAX_IDLE_TIME
							&& curThreadSize_ > initThreadsSize_) {
							// 把线程Thread从当前线程池上删除，
							threads_.erase(threadid);
							curThreadSize_--;
							idleThreadSize_--;
							std::cout << "thread id " << std::this_thread::get_id() << " exit" << std::endl;
							return;
						}
					}
				}
				else
				{
					notEmpty_.wait(lock);
				}
				// Q:为什么非CACHE状态下的，wait阻塞可以结束，
				// A:因为 ThreadPool的析构函数 有notEmpty_.notify_all();
				// 没有任务状态下
				// 如果线程池要结束，在此循环中，taskQue_.size()==0，没有任务，线程是睡着状态
			/*	if (!isPoolRunning_) {
					threads_.erase(threadid);
					if (PoolMode::CACHED == ThreadPool::poolMode_) {
						std::cout << " CACHED ";
					}
					else {
						std::cout << " FIXED ";
					}
					std::cout << "thread id " << std::this_thread::get_id() 
						<< " exit " << std::endl;
					exitCond_.notify_all();
					return; 
				}*/
			}
			
			idleThreadSize_--;
			std::cout << "tid: " << std::this_thread::get_id()
				<< "获取任务 成功！" << std::endl;

			//exitCond_.notify_all();
			// 从任务队列里取出一个任务
			cur_task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;
			if (taskQue_.size() > 0) {
				notEmpty_.notify_all();
			}
			notFull_.notify_all();
		}
		// 当前线程负责处理这个任务
		if (cur_task != nullptr) {
			cur_task->exec();
		}
		idleThreadSize_++;	// 线程执行完毕 空闲线程数量+1
		lastTime = std::chrono::high_resolution_clock().now(); // 更新进程调度完的时间	
	}
	// 正在执行任务的线程，等其执行完进不去上个循环，来到这exit
	
}
bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}
void ThreadPool::start(int initTHreadSize)
{
	isPoolRunning_ = true;
	// 初始线程数数量
	this->initThreadsSize_ = initTHreadSize;
	curThreadSize_ = initTHreadSize;
	// 创建线程对象
	for (int i = 0; i < initTHreadSize; i++) {
		// Thread的构造函数Thread(ThreadFunc func); 
		// new Thread(std::bind(&ThreadPool::threadFunc, this))
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId_ = ptr->getId();
		threads_.emplace(threadId_, std::move(ptr));	
	}
	// 启动线程 std::vector<Thread*>threads_;
	for (int i = 0; i < initTHreadSize; i++) {
		threads_[i]->start();
		idleThreadSize_++;
	}
}

int Thread::generateId_ = 0;
Thread::Thread(ThreadFunc func)
	:func_(func)
	,threadId_(generateId_++)
{
}

Thread::~Thread()
{
}

int Thread::getId() const
{
	return threadId_;
}



void Thread::start()
{
	//创建一个线程来执行 一个线程函数
	std::thread t(func_, threadId_);
	t.detach();  // 设置线程分离

}
// 如果有默认参数，如isValid，在声明处写就可以，不用在定义处写，会报错的
Result::Result(std::shared_ptr<Task> task, bool isValid)
	:task_(task), isValid_(isValid)
{
	task_->setResult(this);
}

void Result::setValue(Any any)
{
	// 获取任务完成后的返回值
	this->any_ = std::move(any);
	// post，加操作
	sem_.post();
}

// 得到返回值
Any Result::get()
{
	if (isValid_ == false) {
		return "";
	}
	// 使用信号 阻塞
	sem_.wait();
	// std::move的作用是将 "左值引用" 转化为  "右值引用"，将对象资源所有权从一个对象转移到另外一个对象，
	// 避免昂贵的复制操作
	return std::move(any_);
}

Task::Task()
	:result_(nullptr)
{
}

void Task::exec()
{
	if (result_ != nullptr) {
		result_->setValue(run());
	}
}

void Task::setResult(Result* result)
{
	result_ = result;
}
