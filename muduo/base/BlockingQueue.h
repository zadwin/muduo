// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_BLOCKINGQUEUE_H
#define MUDUO_BASE_BLOCKINGQUEUE_H

#include "muduo/base/Condition.h"
#include "muduo/base/Mutex.h"

#include <deque>
#include <assert.h>

namespace muduo
{

template<typename T>
class BlockingQueue : noncopyable
{
 public:
  using queue_type = std::deque<T>;

  BlockingQueue()
    : mutex_(),
      notEmpty_(mutex_),
      queue_()   // 无界队列。
  {
  }

  void put(const T& x)
  {
    MutexLockGuard lock(mutex_);
    queue_.push_back(x);
    notEmpty_.notify(); // wait morphing saves us
    // http://www.domaigne.com/blog/computing/condvars-signal-with-mutex-locked-or-not/
  }

  void put(T&& x)   // 生产产品。
  {
    MutexLockGuard lock(mutex_); // 对象的生存周期是花括号内。
    queue_.push_back(std::move(x));
    notEmpty_.notify(); // 通知消费者。这个语句可以不在保护的范围内。
  }

  T take()
  {   // 消费。
    MutexLockGuard lock(mutex_);
    // always use a while-loop, due to spurious wakeup
    while (queue_.empty())
    {
      notEmpty_.wait();
    }
    assert(!queue_.empty());
    T front(std::move(queue_.front()));  // 这里都是采用直接右值获取，而不是复制。
    queue_.pop_front();
    return front;
  }

  queue_type drain()
  {  // 获取整个队列。
    std::deque<T> queue;
    {
      MutexLockGuard lock(mutex_);
      queue = std::move(queue_);
      assert(queue_.empty());
    }
    return queue;
  }

  size_t size() const
  {   // 队列的大小。注意这里的mutex_需要设置为可变的。
    MutexLockGuard lock(mutex_);
    return queue_.size();
  }

 private:
  mutable MutexLock mutex_;
  Condition         notEmpty_ GUARDED_BY(mutex_); // 一个条件变量。
  queue_type        queue_ GUARDED_BY(mutex_);
};  // __attribute__ ((aligned (64)));

}  // namespace muduo

#endif  // MUDUO_BASE_BLOCKINGQUEUE_H
