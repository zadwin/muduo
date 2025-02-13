// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_TIMER_H
#define MUDUO_NET_TIMER_H

#include "muduo/base/Atomic.h"
#include "muduo/base/Timestamp.h"
#include "muduo/net/Callbacks.h"

namespace muduo
{
namespace net
{

///
/// Internal class for timer event.
//  只是对定时操作的高层次抽象（并没有去调用系统级别的定时器函数），这就代表了是一个定时器类。
///
class Timer : noncopyable
{
 public:
  Timer(TimerCallback cb, Timestamp when, double interval)
    : callback_(std::move(cb)),
      expiration_(when),
      interval_(interval),
      repeat_(interval > 0.0),
      sequence_(s_numCreated_.incrementAndGet())  // 这是一个原子操作，保证sequence_的唯一性。
  { }

  void run() const{                     // 执行定时器的回调函数。
    callback_();
  }

  Timestamp expiration() const  { return expiration_; }
  bool repeat() const { return repeat_; }
  int64_t sequence() const { return sequence_; }
  // 如果是重复的定时器，会调用这个函数，重新启动它。
  void restart(Timestamp now);

  static int64_t numCreated() { return s_numCreated_.get(); }

 private:
  const TimerCallback callback_;    // 定时器回调函数。
  Timestamp expiration_;               // 下一次的超时时刻。
  const double interval_;                // 超时时间间隔，如果是一次性定时器，该值为0.
  const bool repeat_;                     // 是否重复。
  const int64_t sequence_;            // 定时器序号。

  static AtomicInt64 s_numCreated_;   // 定时器计数，当前已经创建的定时器数量。这是一个原子性的。
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_TIMER_H
