// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/ThreadPool.h"

#include "muduo/base/Exception.h"

#include <assert.h>
#include <stdio.h>

using namespace muduo;

ThreadPool::ThreadPool(const string& nameArg)
  : mutex_(),
    notEmpty_(mutex_),  // 为空的条件变量。
    notFull_(mutex_),      // 是否满的条件变量。条件变量都需要和互斥变量进行绑定。
    name_(nameArg),
    maxQueueSize_(0),
    running_(false)
{
}

ThreadPool::~ThreadPool()
{
  if (running_)
  {
    stop();
  }
}

// 需要指定线程的数量。
void ThreadPool::start(int numThreads)
{
  assert(threads_.empty());
  running_ = true;
  threads_.reserve(numThreads);
  for (int i = 0; i < numThreads; ++i)
  {
    char id[32];
    snprintf(id, sizeof id, "%d", i+1);
    // 创建指定数量的线程。
    threads_.emplace_back(new muduo::Thread(
          std::bind(&ThreadPool::runInThread, this), name_+id));
    threads_[i]->start();
  }
  // 如果没有线程、但是有任务，这个时候就可以让主线程去执行程序。
  if (numThreads == 0 && threadInitCallback_)
  {
    threadInitCallback_();
  }
}

void ThreadPool::stop()
{
  {
  MutexLockGuard lock(mutex_);
  running_ = false;
  notEmpty_.notifyAll();
  notFull_.notifyAll();
  }
  for (auto& thr : threads_)
  {
    thr->join();
  }
}

size_t ThreadPool::queueSize() const
{
  MutexLockGuard lock(mutex_);
  return queue_.size();
}

void ThreadPool::run(Task task)
{
  if (threads_.empty())
  { // 如果没有线程，那就让主线程运行。如果没有线程那么也就刻意不用互斥变量了。
    task();
  }
  else
  { // 如果有线程，则让线程去运行。
    MutexLockGuard lock(mutex_);
    while (isFull() && running_)
    { // 如果已经满了，那么该任务就没法加进去，需要等待该任务不满。
      notFull_.wait();
    }
    if (!running_) return;
    assert(!isFull());
    // 然后就表明不满，这个时候就可以将任务添加到队列了。
    queue_.push_back(std::move(task));
    notEmpty_.notify();   // 并且通知所有的线程，此时有任务可以运行了。
  }
}

ThreadPool::Task ThreadPool::take()
{// 执行任务的时候也是要进行互斥锁定的。
  MutexLockGuard lock(mutex_);
  // always use a while-loop, due to spurious wakeup
  while (queue_.empty() && running_)
  {
    notEmpty_.wait();
  }
  Task task;
  if (!queue_.empty())
  {
    task = queue_.front();
    queue_.pop_front();
    if (maxQueueSize_ > 0)
    {
      notFull_.notify();    //
    }
  }
  return task;
}

bool ThreadPool::isFull() const
{ // 判断任务队列是否已经满了。
  mutex_.assertLocked();                    // 断言当前线程是否拥有该锁。
  return maxQueueSize_ > 0 && queue_.size() >= maxQueueSize_;
}

void ThreadPool::runInThread()
{
  try
  {
    if (threadInitCallback_)
    {   // 如果每个线程发现有任务，就去获取任务，并且执行。
      threadInitCallback_();
    }
    while (running_)
    { // 不断地去获取任务，不断地执行。
      Task task(take()); // 获取任务。
      if (task)
      { // 获取任务，并且执行。
        task();
      }
    }
  }
  catch (const Exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    abort();
  }
  catch (const std::exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    abort();
  }
  catch (...)
  {
    fprintf(stderr, "unknown exception caught in ThreadPool %s\n", name_.c_str());
    throw; // rethrow
  }
}

