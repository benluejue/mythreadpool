#ifndef THREAD_POOL
#define THREAD_POOL
#include <vector>
#include <memory>
#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <future>
#include <thread>
#include <iostream>
const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THREADHILD = 10;
const int THREAD_MAX_IDLE_TIME = 10;

enum class PoolMode {
	FIXED,
	CACHED
};
class Thread {
public:
	//绑定的 这个函数void ThreadPool​::();
	using ThreadFunc = std::function<void(int)>;
	//开启线程
	void start() {
		//创建一个线程来执行 一个线程函数
		std::thread t(func_, threadId_);
		t.detach();  // 设置线程分离

	}
	//线程构造
	Thread(ThreadFunc func) 
		:func_(func)
		, threadId_(generateId_++)
	{
	}
	//线程析构
	~Thread()=default;

	// 获取线程ID
	int getId()const
	{
		return threadId_;
	}

private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_; // 保存线程ID
};
int Thread::generateId_ = 0;
class ThreadPool
{
public:
	ThreadPool()
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
	~ThreadPool()
	{
		// 等待所有线程结束
		// 两种状态，1阻塞，2 正在进行任务
		isPoolRunning_ = false;
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		notEmpty_.notify_all();
		exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });

	}
	

	//开启线程池 
	void start(int initTHreadSize = std::thread::hardware_concurrency()) {
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
	//设置模式
	void setMode(PoolMode mode) {
		// 运行中不允许设置模式
		if (checkRunningState())
			return;
		poolMode_ = mode;
	}
	
	// 设置 任务队列的最大上限
	void setTaskQueMaxThreadHould(int threshould) {
		if (checkRunningState())
			return;
		this->taskQueMaxThread_ = threshould;
	}
	
	//设置 初始线程数量
	void setInitThreadSize(int size) {
		if (checkRunningState())
			return;
		this->initThreadsSize_ = size;
	}
	// 设置线程cache模式下线程阈值
	void setThreadSizeThreadHould(int threadhold) {
		if (checkRunningState())
			return;
		if (poolMode_ == PoolMode::CACHED) {
			this->threadSizeThresHold_ = threadhold;
		}
	}
	// 提交任务 用户调用该接口 传入任务对象 生成任务
	// shared_ptr 与 unique_ptr的区别为 unique在某一时刻只能指向一个对象
	
	// Result submitTask(std::shared_ptr<Task> sp);
	// pool.submitTask(sum1,10,20), 这里使用Args&&是引用折叠，除了输入一个右值，类型还是右值
	// 其他情况下 会变成左值引用
	// decltype推导表达式的返回值  
	// 我们不知道返回值类型，因此使用 decltype(func(args...)) 来求返回值类型
	template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
{
		using RType = decltype(func(args...));
		// std::forward保持左右值特性
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
			);
		std::future<RType> result = task->get_future();
		// 获取锁
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		
		if (!notFull_.wait_for(lock, std::chrono::seconds(1),
			[&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThread_; })) {
			// 如果返回false 说明在这1s 
			std::cout << "任务提交失败!!!!!" << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]()->RType {return RType(); }
				);
			(*task);
			return task->get_future();
		}
		// 如果有空余 把 任务 放入 任务队列中
		// taskQue_.emplace(sp);
		// using Task = std::function<void()>;
		// std::queue<Task>taskQue_; 也就是说我们可以把任意的void()函数传递进来
		// (*task)() 调用了 std::packaged_task 中封装的任务函数
		taskQue_.emplace([task]() {(*task)();});

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

	 return result;
}
	
	// 线程处理函数 线程池的所有线程从 任务队列里 消费任务
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool operator=(const ThreadPool&) = delete;
	//	定义线程函数
private:
	void threadFunc(int threadid) 
	{
		// resolution 分辨率
		auto lastTime = std::chrono::high_resolution_clock().now();
		for (;;)
		{
			Task cur_task;
			{
				// 先获取锁
				std::unique_lock<std::mutex> lock(taskQueMtx_);
				std::cout << "tid: " << std::this_thread::get_id()
					<< "尝试获取任务！" << std::endl;
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
				// 执行function<void()>
				cur_task();
				
			}
			idleThreadSize_++;	// 线程执行完毕 空闲线程数量+1
			lastTime = std::chrono::high_resolution_clock().now(); // 更新进程调度完的时间	
		}
		// 正在执行任务的线程，等其执行完进不去上个循环，来到这exit

	}
	bool checkRunningState() const 
	{
		return isPoolRunning_;
	}

private:
	// 新的线程列表
	std::unordered_map<int, std::unique_ptr<Thread>>threads_;
	// std::vector<std::unique_ptr<Thread>>threads_;// 线程列表
	int initThreadsSize_;		// 初始线程数量
	int threadSizeThresHold_;	// 线程数量上限

	// 任务 任务对象 我们无法直接和任务函数关联，但是可以使用中间层让两者关联起来
	using Task = std::function<void()>;
	std::queue<Task>taskQue_;// 任务队列
	std::atomic_int idleThreadSize_;// 记录空闲线程池数量
	std::atomic_int curThreadSize_;// 当前线程池中线程的数量
	std::atomic_int taskSize_;	// 任务的数量
	size_t taskQueMaxThread_;	// 任务的数量上限
	std::mutex taskQueMtx_;		// 保障任务队列线程安全的锁
	std::condition_variable notFull_, notEmpty_;
	PoolMode poolMode_;			// 线程工作模式
	std::atomic_bool isPoolRunning_;// 线程池工作状态
	std::condition_variable exitCond_; // 析构时候的条件变量
};


#endif // !THREAD_POOL

