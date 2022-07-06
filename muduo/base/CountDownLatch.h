// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_COUNTDOWNLATCH_H
#define MUDUO_BASE_COUNTDOWNLATCH_H

#include "muduo/base/Condition.h"
#include "muduo/base/Mutex.h"

namespace muduo
{
  // 这个是一个“倒计时门闩”同步。
  // 一种常用且易用的同步手段。
  class CountDownLatch : noncopyable
  {
  public:
    explicit CountDownLatch(int count);
    // 等待条件满足。
    void wait();
    // count--。
    void countDown();
    // 获取count的值。
    int getCount() const;

  private:
    // 用mutable修饰就可以使得getCount可以改变它的状态。
    mutable MutexLock mutex_;  // 加锁是为了能够使得count不会被同时访问。
    Condition condition_ GUARDED_BY(mutex_); // 这里只是一个引用。
    int count_ GUARDED_BY(mutex_); // 倒计时。
};

}  // namespace muduo
#endif  // MUDUO_BASE_COUNTDOWNLATCH_H
