// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_EVENTLOOPTHREAD_H
#define MUDUO_NET_EVENTLOOPTHREAD_H

#include "muduo/base/Condition.h"
#include "muduo/base/Mutex.h"
#include "muduo/base/Thread.h"

namespace muduo
{
namespace net
{

class EventLoop;

// 线程IO类。
class EventLoopThread : noncopyable{
 public:
  typedef std::function<void(EventLoop*)> ThreadInitCallback;
  EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                  const string& name = string());
  ~EventLoopThread();
  EventLoop* startLoop();                             // 启动线程，该线程就成为了IO线程。

 private:
  void threadFunc();                                    // 线程函数

  EventLoop* loop_ GUARDED_BY(mutex_);  // 初始的时候还没有Eventloop对象，需要启动线程后。。
  bool exiting_;      // 是否退出。
  Thread thread_;   // 采用的是基于对象的编程思想，包含了一个线程对象。
  // 信号和条件变量一直是配合使用的。
  MutexLock mutex_;
  Condition cond_ GUARDED_BY(mutex_);     // 条件变量。
  ThreadInitCallback callback_;     // 线程初始化的回调函数，事件循环之前被调用。
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_EVENTLOOPTHREAD_H

