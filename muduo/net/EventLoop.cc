// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/EventLoop.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/Channel.h"
#include "muduo/net/Poller.h"
#include "muduo/net/SocketsOps.h"
#include "muduo/net/TimerQueue.h"

#include <algorithm>

#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

namespace
{
//  当前线程EventLoop对象指针，线程局部存储，每个线程对象都有一份。
__thread EventLoop* t_loopInThisThread = 0;

const int kPollTimeMs = 10000;  // 10秒。

int createEventfd()
{
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0)
  {
    LOG_SYSERR << "Failed in eventfd";
    abort();
  }
  return evtfd;
}

#pragma GCC diagnostic ignored "-Wold-style-cast"
class IgnoreSigPipe
{
 public:
  IgnoreSigPipe()
  {
    ::signal(SIGPIPE, SIG_IGN);
    // LOG_TRACE << "Ignore SIGPIPE";
  }
};
#pragma GCC diagnostic error "-Wold-style-cast"

IgnoreSigPipe initObj;
}  // namespace

EventLoop* EventLoop::getEventLoopOfCurrentThread()
{
  return t_loopInThisThread;
}

EventLoop::EventLoop()
  : looping_(false),                            // 初始化还没处于循环的状态。
    quit_(false),
    eventHandling_(false),
    callingPendingFunctors_(false),
    iteration_(0),
    threadId_(CurrentThread::tid()),      // 当前线程的真实id
    poller_(Poller::newDefaultPoller(this)),    // 相当于是在这里继承了，然后子进程就可以调用了。这里就用到了向上转型模式了。可以是poll也可以是epoll。
    timerQueue_(new TimerQueue(this)),    // 一开始就有这样一个队列了，只有它现在就注册。
    wakeupFd_(createEventfd()),               // 创建了 eventfd 以及创建了Channel对象。
    wakeupChannel_(new Channel(this, wakeupFd_)),
    currentActiveChannel_(NULL) {
  LOG_DEBUG << "EventLoop created " << this << " in thread " << threadId_;    // 记录日志。
  // 如果当前线程已经创建了EventLoop对象，则终止该程序。
  if (t_loopInThisThread){
    LOG_FATAL << "Another EventLoop " << t_loopInThisThread
              << " exists in this thread " << threadId_;
  }else{
    t_loopInThisThread = this;
  }
  // 这样唤醒完，就可以防止eventloop一直阻塞了，这样也就可以执行一些其他的函数任务了。
  wakeupChannel_->setReadCallback(
      std::bind(&EventLoop::handleRead, this));
  // we are always reading the wakeupfd
  wakeupChannel_->enableReading();    // 此时wakeup也被注册了进入poll了。
}

EventLoop::~EventLoop(){
  LOG_DEBUG << "EventLoop " << this << " of thread " << threadId_
            << " destructs in thread " << CurrentThread::tid();
  wakeupChannel_->disableAll();
  wakeupChannel_->remove();
  ::close(wakeupFd_);
  t_loopInThisThread = NULL;
}

// 事件循环，该函数不能跨线程调用。
void EventLoop::loop(){
  assert(!looping_);                                                                        // 断言线程是否启动了事件循环。
  assertInLoopThread();                                                                 // 断言当前调用是不是在该对应线程中。
  looping_ = true;
  quit_ = false;  // FIXME: what if someone calls quit() before loop() ?
  LOG_TRACE << "EventLoop " << this << " start looping";              // 记录日志，启动本次的IO事件。
  while (!quit_){
    activeChannels_.clear();                                                            // 清空上一轮的活动通道。
    // 这里设置的超时时间是10s，如果超过了这个时间还是没有事件到来也会返回这个函数。
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_); // 执行一次活跃事件的获取（只有活跃的事件），并返回一个时间戳。
    ++iteration_;                                                                           // 记录事件循环的迭代次数。
    if (Logger::logLevel() <= Logger::TRACE){       // 这里其实就是去判断当前日志的等级。
      printActiveChannels();                                                            // 打印活动通道。
    }
    // TODO sort channel by priority
    eventHandling_ = true;                                                              // 执行事件处理。
    for (Channel* channel : activeChannels_){                                   // 循环处理每一个事件，其中的handleEvent事件是每个独有的处理方式。
      currentActiveChannel_ = channel;                                            // 记录一下当前的正在处理的通道。
      currentActiveChannel_->handleEvent(pollReturnTime_);              // 开始处理该通道的事件，此时还是处于IO线程中。
    }
    currentActiveChannel_ = NULL;                                                 // 处理完后将当前处理的通道置为空。
    eventHandling_ = false;                                                            // 标记当前没有通道在处理。
    // 这里一定不能无限的执行dopendingfunctors。通过这种方式可以实现线程安全的异步调用。
    doPendingFunctors();                                                               // 其他线程或者IO线程添加的一些任务，让IO线程也能执行一些计算任务。
  }
  LOG_TRACE << "EventLoop " << this << " stop looping";               // 这里就表示一个EventLoop停止了，并不表示被销毁了。
  looping_ = false;                                                                        // loop停止了。
}
// 该线程可以跨线程调用，如果是在其他线程调用，则需要唤醒。
// 因为如果不是在当前线程中调用，可能其他eventloop线程正处于handle的状态。
void EventLoop::quit(){
  quit_ = true;     // bool类型本身就是原子性类，因此不需要保护，整型的操作就要用到原子性操作。
  // There is a chance that loop() just executes while(!quit_) and exits,
  // then EventLoop destructs, then we are accessing an invalid object.
  // Can be fixed using mutex_ in both places.
  if (!isInLoopThread()){   // 如果不是在loop线程中调用quit函数，还需要调用wakeup函数。
    wakeup();                  // 这里其实就是一个唤醒evnetloop的机制，通过一个eventfd，这样才能执行到while起始位置。
  }
}
// 用来接收eventloop该处理的函数。
void EventLoop::runInLoop(Functor cb){
  if (isInLoopThread()){              // 本IO线程同步调用。
    cb();
  }else{  // 如果不是在eventloop线程内部。其他线程，则需要异步地将cb添加到队列当中。
    queueInLoop(std::move(cb));                 // 以便让eventloop对象的线程来调用cb函数。
  }
}

void EventLoop::queueInLoop(Functor cb)
{
  {
  // 需要通过互斥量保护临界区。
  MutexLockGuard lock(mutex_);
  pendingFunctors_.push_back(std::move(cb));        // 将要执行的函数添加到一个队列中，以便统一执行。
  }
  // 如果不是在本线程内，并且函数队列有函数需要执行，则唤醒eventloop线程。
  // 以下是需要唤醒的几种情况。如果是当前线程并且这在调用callingPendingFunctors也要唤醒（这种情况也就只有是IO线程中的pendingfunctors调用了wakeup）。
  if (!isInLoopThread() || callingPendingFunctors_){
    wakeup();     // 唤醒的操作，也就是可以跳过poll函数的等待。
  }
}
size_t EventLoop::queueSize() const
{
  MutexLockGuard lock(mutex_);
  return pendingFunctors_.size();
}
// 通过eventloop的线程去增加定时器，主要是防止多个线程重复添加，这里也就可以更加充分的利用eventloop线程。
TimerId EventLoop::runAt(Timestamp time, TimerCallback cb){
  return timerQueue_->addTimer(std::move(cb), time, 0.0); //  0.0 表示不是一个重复的定时器。
}
// 通过eventloop去设置定时器，eventloop对象管理了一个timerqueue队列。z
TimerId EventLoop::runAfter(double delay, TimerCallback cb){
  Timestamp time(addTime(Timestamp::now(), delay));
  return runAt(time, std::move(cb));
}

TimerId EventLoop::runEvery(double interval, TimerCallback cb)
{
  Timestamp time(addTime(Timestamp::now(), interval));          // 就是为了得到一个时间而已。
  return timerQueue_->addTimer(std::move(cb), time, interval); // timerqueue生成一个定时器对象。
}

void EventLoop::cancel(TimerId timerId)
{
  return timerQueue_->cancel(timerId);
}
// 调用更新事件的监听模式。
void EventLoop::updateChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();     // 防止在其他线程执行这个操作。
  poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);           // 判断这个通道是否是有当前eventloop负责。
  assertInLoopThread();                                   // 断言调用当前还是是否是在loopthread中。
  if (eventHandling_)
  {
    assert(currentActiveChannel_ == channel ||
        std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
  }
  poller_->removeChannel(channel);      // 然后将这个channel交给poller负责处理。
}

bool EventLoop::hasChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  return poller_->hasChannel(channel);
}

void EventLoop::abortNotInLoopThread()
{
  LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
            << " was created in threadId_ = " << threadId_
            << ", current thread id = " <<  CurrentThread::tid();
}

void EventLoop::wakeup()
{
  uint64_t one = 1;
  ssize_t n = sockets::write(wakeupFd_, &one, sizeof one);
  if (n != sizeof one){
    LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";   // 日志。
  }
}
// 这个仅仅是用于wakeup事件的。
void EventLoop::handleRead(){
  uint64_t one = 1;
  ssize_t n = sockets::read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
}

void EventLoop::doPendingFunctors()
{
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;

  {
  MutexLockGuard lock(mutex_);
  // 进行了一个交换，但是为什么要交换？减少了临界区的长度（也就是需要保护的区域），这样就不会阻塞其他线程queueinloop()，避免了死锁。
  //
  functors.swap(pendingFunctors_);
  }

  for (const Functor& functor : functors)
  {
    functor();
  }
  callingPendingFunctors_ = false;
}

void EventLoop::printActiveChannels() const
{
  for (const Channel* channel : activeChannels_)
  { // 每一个日志都有一个名称。
    LOG_TRACE << "{" << channel->reventsToString() << "} ";
  }
}

