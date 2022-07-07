// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_CONDITION_H
#define MUDUO_BASE_CONDITION_H

#include "muduo/base/Mutex.h"

#include <pthread.h>

namespace muduo
{

class Condition : noncopyable
{
 public:
  explicit Condition(MutexLock& mutex)
    : mutex_(mutex)
  {
    MCHECK(pthread_cond_init(&pcond_, NULL));
  }

  ~Condition()
  {
    MCHECK(pthread_cond_destroy(&pcond_));
  }

  void wait()
  {
    MutexLock::UnassignGuard ug(mutex_);                            // 先将holder_清零，防止出现死锁。但是如何去表明这个是有效的呢。
    // ug析构的时候，会将holder_置为该线程的tid， 通过互斥变量去绑定条件变量。
    // 其实主要就是为了能够更好的通知各个线程。
    MCHECK(pthread_cond_wait(&pcond_, mutex_.getPthreadMutex()));   // 线程条件等待。
  }

  // returns true if time out, false otherwise.
  bool waitForSeconds(double seconds);

  void notify()
  {
    MCHECK(pthread_cond_signal(&pcond_));   // 表示资源可用。
  }

  void notifyAll()
  {
    MCHECK(pthread_cond_broadcast(&pcond_));  // 通常表示条件变化。条件是不是同一个，是同一个，因为线程会继承进程中的信息。
  }

 private:
  MutexLock& mutex_;  // 锁。
  pthread_cond_t pcond_;  //  线程条件。
};

}  // namespace muduo

#endif  // MUDUO_BASE_CONDITION_H
