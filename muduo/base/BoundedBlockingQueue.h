// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H
#define MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H

#include "muduo/base/Condition.h"
#include "muduo/base/Mutex.h"

#include <boost/circular_buffer.hpp>
#include <assert.h>

namespace muduo
{

template<typename T>
class BoundedBlockingQueue : noncopyable
{
 public:
  explicit BoundedBlockingQueue(int maxSize)
    : mutex_(),
      notEmpty_(mutex_),
      notFull_(mutex_),
      queue_(maxSize)
  {
  }

  void put(const T& x)
  {
    MutexLockGuard lock(mutex_);
    while (queue_.full())
    {
      notFull_.wait();
    }
    assert(!queue_.full());
    queue_.push_back(x);
    notEmpty_.notify();
  }

  void put(T&& x)
  {
    MutexLockGuard lock(mutex_);
    while (queue_.full())
    {
      notFull_.wait(); // 生产者等待队列不满。
    }
    assert(!queue_.full());
    queue_.push_back(std::move(x));
    notEmpty_.notify();  // 需要通知其他线程此时队列不为空。
  }

  T take()
  {
    MutexLockGuard lock(mutex_);
    while (queue_.empty())
    {
      notEmpty_.wait();   // 消费者等待队列不为空。
    }
    assert(!queue_.empty());
    T front(std::move(queue_.front())); // 直接夺取内存。可以避免不必要的拷贝操作。
    queue_.pop_front();
    notFull_.notify();
    return front;
  }

  bool empty() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.empty();
  }

  bool full() const
  { //  判断是否为满。
    MutexLockGuard lock(mutex_);
    return queue_.full();
  }

  size_t size() const
  {  // 判断是否为空。
    MutexLockGuard lock(mutex_);
    return queue_.size();
  }

  size_t capacity() const
  {  // 测试它的容量。
    MutexLockGuard lock(mutex_);
    return queue_.capacity();
  }

 private:
  mutable MutexLock          mutex_;  // 同样这里也要用到mutable关键字来修饰。
  Condition                  notEmpty_ GUARDED_BY(mutex_);
  Condition                  notFull_ GUARDED_BY(mutex_);
  boost::circular_buffer<T>  queue_ GUARDED_BY(mutex_); // 这里采用的是boost库中的环形队列。
};

}  // namespace muduo

#endif  // MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H
