// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include "muduo/net/TimerQueue.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/Timer.h"
#include "muduo/net/TimerId.h"

#include <sys/timerfd.h>
#include <unistd.h>

namespace muduo
{
namespace net
{
namespace detail
{

int createTimerfd()
{
  int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                 TFD_NONBLOCK | TFD_CLOEXEC);
  if (timerfd < 0)
  {
    LOG_SYSFATAL << "Failed in timerfd_create";
  }
  return timerfd;
}

// 计算超时时刻与当前时间的时间差。
struct timespec howMuchTimeFromNow(Timestamp when)
{
  int64_t microseconds = when.microSecondsSinceEpoch()
                         - Timestamp::now().microSecondsSinceEpoch();
  if (microseconds < 100)
  {
    microseconds = 100;
  }
  struct timespec ts;
  ts.tv_sec = static_cast<time_t>(
      microseconds / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = static_cast<long>(
      (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
  return ts;
}

void readTimerfd(int timerfd, Timestamp now)
{
  uint64_t howmany;
  ssize_t n = ::read(timerfd, &howmany, sizeof howmany);    // 将这个timefd的事件给读走。
  LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
  if (n != sizeof howmany){
    LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
  }
}

// 重置定时器的超时时刻。
void resetTimerfd(int timerfd, Timestamp expiration)
{
  // wake up loop by timerfd_settime()
  struct itimerspec newValue;
  struct itimerspec oldValue;
  memZero(&newValue, sizeof newValue);
  memZero(&oldValue, sizeof oldValue);
  newValue.it_value = howMuchTimeFromNow(expiration);
  int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);    // 这里就是改变了定时器的响应时间。
  if (ret){
    LOG_SYSERR << "timerfd_settime()";
  }
}

}  // namespace detail
}  // namespace net
}  // namespace muduo

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::detail;

TimerQueue::TimerQueue(EventLoop* loop)
  : loop_(loop),
    timerfd_(createTimerfd()),
    timerfdChannel_(loop, timerfd_),
    timers_(),
    callingExpiredTimers_(false){
  // 这里就是对与定时器的回调函数的处理方式。
  // 但是这里如何通过这一个文件描述符去处理所有的定时器函数，这里就让eventloop关注起了定时器事件。
  timerfdChannel_.setReadCallback(
      std::bind(&TimerQueue::handleRead, this));  // 这里注册的是一个timerQueue的函数，里面肯定是不断地遍历。
  // we are always reading the timerfd, we disarm it with timerfd_settime.
  timerfdChannel_.enableReading();                  // 启动通道，让 epoll 进行监听。
}

TimerQueue::~TimerQueue()
{
  timerfdChannel_.disableAll();
  timerfdChannel_.remove();
  ::close(timerfd_);
  // do not remove channel, since we're in EventLoop::dtor();
  for (const Entry& timer : timers_)
  {
    delete timer.second;
  }
}

TimerId TimerQueue::addTimer(TimerCallback cb,
                             Timestamp when,  // 超时时间。
                             double interval){    // 间隔时间。
  Timer* timer = new Timer(std::move(cb), when, interval);  // 构造了一个时间的对象。
  // 这里只是借助eventloop的IO线程进行添加定时器，并没有对定时器本身做什么？
  loop_->runInLoop(
      std::bind(&TimerQueue::addTimerInLoop, this, timer)); // 这里运行的是添加timer的函数。
  return TimerId(timer, timer->sequence());
}

void TimerQueue::cancel(TimerId timerId) {
  loop_->runInLoop(
      std::bind(&TimerQueue::cancelInLoop, this, timerId));
}

void TimerQueue::addTimerInLoop(Timer* timer){
  loop_->assertInLoopThread();
  // 插入一个定时器，有可能会是的最早到期的定时器发生改变。
  // 这里也就点破了为什么用一个timefd就能够负责这么多个定时器的缘故。
  bool earliestChanged = insert(timer); // 执行插入操作。 如果插入的倒计时时间更早，那么这里就会为ture。
  if (earliestChanged){                       // 为什么要在当前线程？因为为了防止，多个线程改变了最早的定时器。
    resetTimerfd(timerfd_, timer->expiration());// timerfd_也就是定时器文件描述符。
  }
}
// 取消定时器。
void TimerQueue::cancelInLoop(TimerId timerId)
{
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  ActiveTimer timer(timerId.timer_, timerId.sequence_);
  ActiveTimerSet::iterator it = activeTimers_.find(timer);
  if (it != activeTimers_.end())
  {
    size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
    assert(n == 1); (void)n;
    delete it->first; // FIXME: no delete please
    activeTimers_.erase(it);
  }else if (callingExpiredTimers_){
    cancelingTimers_.insert(timer);
  }
  assert(timers_.size() == activeTimers_.size());
}
// timerfd的handlereand函数，都是类似遍历。
void TimerQueue::handleRead()
{
  loop_->assertInLoopThread();
  Timestamp now(Timestamp::now());
  readTimerfd(timerfd_, now);     // 清除该时间，避免一直触发。
  // 获得定时器时刻之前所有超时的timer定时器。
  std::vector<Entry> expired = getExpired(now);

  callingExpiredTimers_ = true;
  cancelingTimers_.clear();
  // safe to callback outside critical section
  for (const Entry& it : expired){    // 读这个队列。
    it.second->run();   // 调用定时器的回调函数。
  }
  callingExpiredTimers_ = false;
  // 如果不是一次性定时器，需要重启。
  reset(expired, now);
}
// 这里就是定时器的handleRead函数。
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
  assert(timers_.size() == activeTimers_.size());
  std::vector<Entry> expired;
  Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
  // 返回第一个未到期的timer的迭代器。
  TimerList::iterator end = timers_.lower_bound(sentry);
  assert(end == timers_.end() || now < end->first);
  // 将到期的定时器插入到expired中。运用到了插入迭代器。
  std::copy(timers_.begin(), end, back_inserter(expired));
  // 从timers_中移除到期的定时器。
  timers_.erase(timers_.begin(), end);

  // 从activeTimers_中移除到期的定时器。
  for (const Entry& it : expired){
    ActiveTimer timer(it.second, it.second->sequence());
    size_t n = activeTimers_.erase(timer);
    assert(n == 1); (void)n;
  }

  assert(timers_.size() == activeTimers_.size());
  return expired;
}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
  Timestamp nextExpire;

  for (const Entry& it : expired)
  {
    ActiveTimer timer(it.second, it.second->sequence());
    // 如果是重复的定时器并且是没有取消的，则重启该定时器。
    if (it.second->repeat()
        && cancelingTimers_.find(timer) == cancelingTimers_.end()) {
      it.second->restart(now);
      insert(it.second);
    }else{
      // FIXME move to a free list
      // 一次性定时器不能重置，因此删除该定时器。
      delete it.second; // FIXME: no delete please
    }
  }

  if (!timers_.empty())
  {
    nextExpire = timers_.begin()->second->expiration();
  }

  if (nextExpire.valid())
  {
    resetTimerfd(timerfd_, nextExpire);
  }
}
// 插入一个timer，判断是否改变了最早的定时器。
bool TimerQueue::insert(Timer* timer)
{
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  // 最早到期时间是否改变。
  bool earliestChanged = false;
  Timestamp when = timer->expiration();
  TimerList::iterator it = timers_.begin(); // 这里这是有序的。
  // 如果timer_为空或者when小于timers_中的最早到期时间。
  if (it == timers_.end() || when < it->first){
    earliestChanged = true;
  }
  {
    // 插入到timers_中。
    std::pair<TimerList::iterator, bool> result
      = timers_.insert(Entry(when, timer));
    assert(result.second); (void)result;
  }
  {
    // 插入到activerTimer中。
    std::pair<ActiveTimerSet::iterator, bool> result
      = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
    assert(result.second); (void)result;
  }
  assert(timers_.size() == activeTimers_.size());
  return earliestChanged;
}

