// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/EventLoopThread.h"

#include "muduo/net/EventLoop.h"

using namespace muduo;
using namespace muduo::net;

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const string& name)
  : loop_(NULL),
    exiting_(false),
    thread_(std::bind(&EventLoopThread::threadFunc, this), name), // 相当于这里就以线程的方式创建了一个类了。
    mutex_(),
    cond_(mutex_),
    callback_(cb)         // 初始化回调函数。
{
}

EventLoopThread::~EventLoopThread()
{
  exiting_ = true;
  if (loop_ != NULL) // not 100% race-free, eg. threadFunc could be running callback_.
  {
    // still a tiny chance to call destructed object, if threadFunc exits just now.
    // but when EventLoopThread destructs, usually programming is exiting anyway.
    loop_->quit();    // 退出loop循环，从而退出IO线程。
    thread_.join();
  }
}

EventLoop* EventLoopThread::startLoop()
{
  assert(!thread_.started());
  thread_.start();  // 线程启动，调用线程的回调函数。这个时候就有两个线程分支。

  EventLoop* loop = NULL;
  {
    MutexLockGuard lock(mutex_);
    while (loop_ == NULL)
    {
      cond_.wait(); // 等待该信号。
    }
    loop = loop_;
  }

  return loop;
}
// 这个函数退出了，就意味着线程退出了，eventloop对象也就不存在了。
void EventLoopThread::threadFunc()
{
  EventLoop loop; // 创建一个eventloop实例。创建是一定会创建的。

  if (callback_){
    callback_(&loop);
  }
  {
    // 相当于是一个信号保护。
    MutexLockGuard lock(mutex_);      // 利用栈的思想来释放锁。
    loop_ = &loop;
    cond_.notify();   // 通知线程资源可用，也就是这个loop_。
  }

  loop.loop();                                  // 启动了一个loop函数。
  //assert(exiting_);
  MutexLockGuard lock(mutex_);      // 关闭线程。
  loop_ = NULL;
}

