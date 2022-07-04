// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)
/**
 * @brief   这里主要是线程对象。
 *
 */


#ifndef MUDUO_BASE_THREAD_H
#define MUDUO_BASE_THREAD_H

#include "muduo/base/Atomic.h"
#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Types.h"

#include <functional>
#include <memory>
#include <pthread.h>

namespace muduo
{
// 包含更多的是功能属性，有关线程的信息在另一个类中获得。
class Thread : noncopyable
{
 public:
 //需要接收的是一个无参数的。
  typedef std::function<void ()> ThreadFunc;  // 回调的函数的定义，因为用的是boost::function

  explicit Thread(ThreadFunc, const string& name = string());
  // FIXME: make it movable in C++11
  ~Thread();

  void start();
  int join(); // return pthread_join()

  bool started() const { return started_; }
  // pthread_t pthreadId() const { return pthreadId_; }
  pid_t tid() const { return tid_; }
  const string& name() const { return name_; }

  static int numCreated() { return numCreated_.get(); }

 private:
  void setDefaultName();

  bool       started_;  // 线程是否已经启动了。
  bool       joined_; // 线程是否被join了 。
  // 注意区分真实id和线程id。
  pthread_t  pthreadId_;  // 线程的id。
  pid_t      tid_;  // 线程的真实pid
  ThreadFunc func_; // 回调函数。
  string     name_; // 线程的名称。
  CountDownLatch latch_;
  // 这里就是一个int类型的意思。
  static AtomicInt32 numCreated_; // 创建线程的数量。这里都只是声明，静态成员变量。
};

}  // namespace muduo
#endif  // MUDUO_BASE_THREAD_H
