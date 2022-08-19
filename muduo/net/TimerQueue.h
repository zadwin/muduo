// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_TIMERQUEUE_H
#define MUDUO_NET_TIMERQUEUE_H

#include <set>
#include <vector>

#include "muduo/base/Mutex.h"
#include "muduo/base/Timestamp.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/Channel.h"

namespace muduo
{
namespace net
{

class EventLoop;
class Timer;
class TimerId;

///
/// A best efforts timer queue.
/// No guarantee that the callback will be on time.
// 定时器的队列，里面维护了一个列表。
///
class TimerQueue : noncopyable
{
 public:
  explicit TimerQueue(EventLoop* loop);     // 这个对象属于一个EventLoop对象
  ~TimerQueue();

  ///
  /// Schedules the callback to be run at given time,
  /// repeats if @c interval > 0.0.
  ///
  /// Must be thread safe. Usually be called from other threads.
  // 添加一个定时器。一定是线程安全的，可以跨线程调用，通常情况下被其他线程调用。
  // 因此这些函数的调用需要加锁。
  TimerId addTimer(TimerCallback cb,
                   Timestamp when,
                   double interval);
// 取消一个定时器。
  void cancel(TimerId timerId);

 private:

  // FIXME: use unique_ptr<Timer> instead of raw pointers.
  // This requires heterogeneous comparison lookup (N3465) from C++14
  // so that we can find an T* in a set<unique_ptr<T>>.
  typedef std::pair<Timestamp, Timer*> Entry;
  typedef std::set<Entry> TimerList;      // set，按照时间戳来排序。
  typedef std::pair<Timer*, int64_t> ActiveTimer;
  typedef std::set<ActiveTimer> ActiveTimerSet; // 和TimerList保存的是相同的东西，排序的方式不同。

//  以下成员函数只可能在其所属的IO线程中调用，因而不必加锁。
//  服务器性能杀手之一是锁竞争，所以要尽可能少用锁。
  void addTimerInLoop(Timer* timer);
  void cancelInLoop(TimerId timerId);
  // called when timerfd alarms
  void handleRead();
  // move out all expired timers
  // 返回超时的定时器列表，是一个向量。
  std::vector<Entry> getExpired(Timestamp now);
  // 重置超时器列表。因为这些超时的定时器可能存在重复定时器。
  void reset(const std::vector<Entry>& expired, Timestamp now);

  bool insert(Timer* timer);

  EventLoop* loop_;     // 所属EventLoop
  // 是通过这一个timerfd来监控所有的定时器吗？如何监控？
  // 是的，通过这一个timerfd来监控所有的定时器，每次只会监听最早的定时器（因此是需要排序的）。
  const int timerfd_;     // 定时器的文件描述符。当定时器条件到来的时候就会产生handleRead回调函数。
  Channel timerfdChannel_;    // 定时器的通道。
  // Timer list sorted by expiration
  TimerList timers_;      // 是按到期时间排序的定时器集合set。

  // for cancel()
  ActiveTimerSet activeTimers_;                   // 是按对象地址排序的。
  bool callingExpiredTimers_; /* atomic */    // 是否处于调用超时的定时器。
  ActiveTimerSet cancelingTimers_;             // 保存的是被取消的定时器。
};

}  // namespace net
}  // namespace muduo
#endif  // MUDUO_NET_TIMERQUEUE_H
