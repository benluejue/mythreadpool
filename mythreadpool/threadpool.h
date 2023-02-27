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
// 设计一个any类
class Any {
public:
	Any() = default;
	~Any() = default;
	// =delet 表示 如果企图调用该函数就会生成错误
	// 也就是说禁止对Any进行左值拷贝构造，
	Any(const Any&) = delete;
	// 禁止左值赋值 Any& 表示返回的左值引用
	Any& operator=(const Any&) = delete;
	// 右值拷贝构造 主义这里没有 const
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;
	// 可以接受任意类型的  
	template <typename T>
	//	将任意T包装在派生类里	new <Derive<T>>(data)
	Any(T data) :base_(std::make_unique<Derive<T>>(data))
	{}
	// 可以返回任意类型的变量
	template <typename T>
	T cast_(){
		// 如何从base直到其子类Derive的data成员
		// 基类==》》派生类 RTTI是运行时 类型 识别（Runtime Type Information）
		// RTTI使可以让我们使用 基类指针或者引用（Reference），来检查实际产生的 派生类
		// dynamic_cast将 基类指针的引用转化为派生类
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd==0)
		{
			throw "type is unmatch!";
		}
		return pd->data_;
	}
private:
	// 基类
	class Base {
	public:
		virtual ~Base() = default;
	};
	// 派生类
	template<typename T>
	class Derive : public Base {
		public:
			Derive(T data) : data_(data)
			{}
			T data_;
	};
private:
	// 定义一个基类指针 unique_ptr 把拷贝构造和赋值都delete了
	std::unique_ptr<Base>base_;
};
// 设计一个信号类
class Semaphore
{
public:
	Semaphore(int resLimit=0)
		:resLimit_(resLimit)
		,isExit(false);
	{}
	~Semaphore(){
		isExit = true;
	}
public:
	// P是wait操作，也就是减操作 proberen试探尝试
	void wait() {
		if( isExit )return;
		std::unique_lock<std::mutex>lock(mtx_);
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0;});
		resLimit_--;
	}
	// V是post操作，也就是加操作 verhogen 提高
	void post() {
		std::unique_lock<std::mutex>lock(mtx_);
		resLimit_++;
		// 唤醒所有在cond_条件变量上的线程
		cond_.notify_all();
	}
private:
	int resLimit_;
	std::condition_variable cond_;
	std::mutex mtx_;
	bool isExit;

};

class Task;
class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;
	// 获取任务完成后的返回值
	void setValue(Any any);
	// 得到返回值
	Any get();

private:
	Any any_;
	Semaphore sem_;
	std::shared_ptr<Task>task_;
	std::atomic_bool isValid_;
};
class Task {
// 默认成员函数的 权限是 private 因此不能忘记public
public:
	Task();
	~Task() = default;
	virtual Any run() = 0;
	void exec();
	void setResult(Result* result);
private:
	Result* result_;
};
enum class PoolMode {
	FIXED,
	CACHED
};
class Thread {
public:
	//绑定的 这个函数void ThreadPool​::();
	using ThreadFunc = std::function<void(int)>;
	//开启线程
	void start();
	//线程构造
	Thread(ThreadFunc func);
	//线程析构
	~Thread();

	// 获取线程ID
	int getId()const;
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_; // 保存线程ID
};
class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();

	//开启线程池 
	void start(int initTHreadSize = std::thread::hardware_concurrency());
	//设置模式
	void setMode(PoolMode mode);
	// 设置 任务队列的最大上限
	void setTaskQueMaxThreadHould(int threshould);
	//设置 初始线程数量
	void setInitThreadSize(int size);
	// 设置线程cache模式下线程阈值
	void setThreadSizeThreadHould(int threadhold);
	// 提交任务 用户调用该接口 传入任务对象 生成任务
	// shared_ptr 与 unique_ptr的区别为 unique在某一时刻只能指向一个对象
	Result submitTask(std::shared_ptr<Task> sp);
	// 线程处理函数 线程池的所有线程从 任务队列里 消费任务
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool operator=(const ThreadPool&) = delete;
//	定义线程函数
private:
	void threadFunc(int threadId);
	bool checkRunningState() const;

private:
	// 新的线程列表
	std::unordered_map<int, std::unique_ptr<Thread>>threads_;
	// std::vector<std::unique_ptr<Thread>>threads_;// 线程列表
	int initThreadsSize_;		// 初始线程数量
	int threadSizeThresHold_;	// 线程数量上限
	std::queue<std::shared_ptr<Task>>taskQue_;// 线程队列
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

