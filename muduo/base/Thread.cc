// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/Thread.h"
#include "muduo/base/CurrentThread.h"
#include "muduo/base/Exception.h"
#include "muduo/base/Logging.h"

#include <type_traits>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/unistd.h>

namespace muduo
{
namespace detail
{
// 困惑什么样的程序应该封装在类中？
pid_t gettid()
{
  return static_cast<pid_t>(::syscall(SYS_gettid)); // 调用系统函数来获取真实tid
}

void afterFork()
{
  muduo::CurrentThread::t_cachedTid = 0;
  muduo::CurrentThread::t_threadName = "main";
  CurrentThread::tid();
  // no need to call pthread_atfork(NULL, NULL, &afterFork);
}

class ThreadNameInitializer
{
 public:
  ThreadNameInitializer()
  {
    muduo::CurrentThread::t_threadName = "main";
    CurrentThread::tid();
    // 这个函数的作用就是能够在fork前中后三个阶段运行不同的函数，从而得到更好的函数。
    pthread_atfork(NULL, NULL, &afterFork);
  }
};

ThreadNameInitializer init;

struct ThreadData
{
  typedef muduo::Thread::ThreadFunc ThreadFunc;
  ThreadFunc func_;
  string name_;
  pid_t* tid_;
  CountDownLatch* latch_;

  ThreadData(ThreadFunc func,   // 线程接受的回调函数类型。
             const string& name,     // 线程的名称。
             pid_t* tid,                    // 线程的真实id。
             CountDownLatch* latch)
    : func_(std::move(func)),
      name_(name),
      tid_(tid),
      latch_(latch)
  { }

  void runInThread()
  {
    *tid_ = muduo::CurrentThread::tid();
    tid_ = NULL;
    latch_->countDown();  // 运行前就对其进行了 -- 操作。
    latch_ = NULL;
    // 这里就表示线程启动了。
    muduo::CurrentThread::t_threadName = name_.empty() ? "muduoThread" : name_.c_str();
    ::prctl(PR_SET_NAME, muduo::CurrentThread::t_threadName);
    try
    {
      func_(); // 这里才是执行所谓的回调函数。
      muduo::CurrentThread::t_threadName = "finished";
    }
    catch (const Exception& ex)
    {
      muduo::CurrentThread::t_threadName = "crashed";
      fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
      fprintf(stderr, "reason: %s\n", ex.what());
      fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
      abort();
    }
    catch (const std::exception& ex)
    {
      muduo::CurrentThread::t_threadName = "crashed";
      fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
      fprintf(stderr, "reason: %s\n", ex.what());
      abort();
    }
    catch (...)
    {
      muduo::CurrentThread::t_threadName = "crashed";
      fprintf(stderr, "unknown exception caught in Thread %s\n", name_.c_str());
      throw; // rethrow
    }
  }
};
// 这个是一个普通函数。
void* startThread(void* obj)
{
   // 这里就得到了线程类的所有数据（当成了一个参数），包括可以执行将要执行的函数。
  ThreadData* data = static_cast<ThreadData*>(obj);
  data->runInThread();
  delete data;
  return NULL;
}

}  // namespace detail
// 这里用于存储线程的真实id，这样就可以进行线程间的通信，这么做是为了减少频繁进行系统调用。
void CurrentThread::cacheTid()
{
  if (t_cachedTid == 0)
  {
    t_cachedTid = detail::gettid();
    t_tidStringLength = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);
  }
}
// 判断是否为主线程。
bool CurrentThread::isMainThread()
{
  return tid() == ::getpid(); // 判断是否为主线程。
}

void CurrentThread::sleepUsec(int64_t usec)
{
  struct timespec ts = { 0, 0 };
  ts.tv_sec = static_cast<time_t>(usec / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = static_cast<long>(usec % Timestamp::kMicroSecondsPerSecond * 1000);
  // nanosleep() 函数会导致当前的线程将暂停执行, 直到rqtp参数所指定的时间间隔。
  // 或者在指定时间间隔内有信号传递到当前线程，将引起当前线程调用信号捕获函数或终止该线程。 ::nanosleep(&ts, NULL);
}

// 静态成员变量，是原子性的。
AtomicInt32 Thread::numCreated_;

// 这里是线程的构造函数。
Thread::Thread(ThreadFunc func, const string& n)
  : started_(false),
    joined_(false),
    pthreadId_(0), // 初始为0。
    tid_(0),  // 初始为0。
    func_(std::move(func)),  // 这样就可以避免直接复制了。
    name_(n),
    latch_(1)
{
  setDefaultName();
}

Thread::~Thread() // 析构函数。
{
  if (started_ && !joined_)
  {
    pthread_detach(pthreadId_);
  }
}
// 设置线程的默认名称。
void Thread::setDefaultName()
{ // 线程的默认名称。
  int num = numCreated_.incrementAndGet();
  if (name_.empty())
  {
    char buf[32];
    snprintf(buf, sizeof buf, "Thread%d", num);
    name_ = buf;
  }
}

// 线程的启动函数。这里还没有运行程序，只是处于准备阶段。
void Thread::start()
{
  assert(!started_);
  started_ = true;
  // FIXME: move(func_)
  // 这里相当于是去获取线程的信息。
  detail::ThreadData* data = new detail::ThreadData(func_, name_, &tid_, &latch_);
  // 创建线程，data是是关于线程的参数，是一个ThreadData类。
  if (pthread_create(&pthreadId_, NULL, &detail::startThread, data))
  { // 如果启动失败，则需要释放资源等操作。
    started_ = false;
    delete data; // or no delete?
    LOG_SYSFATAL << "Failed in pthread_create";
  }
  else
  {
    latch_.wait();
    assert(tid_ > 0);
  }
}
// 将线程加入。
int Thread::join()
{
  assert(started_);
  assert(!joined_);
  joined_ = true;
  // 这里就是将现场join，pthread_join()函数，以阻塞的方式等待thread指定的线程结束。当函数返回时，被等待线程的资源被收回。
  return pthread_join(pthreadId_, NULL);
}

}  // namespace muduo
