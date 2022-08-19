#include "muduo/net/EventLoop.h"
#include "muduo/base/Thread.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

EventLoop* g_loop;

void callback()
{
  printf("callback(): pid = %d, tid = %d\n", getpid(), CurrentThread::tid());
  EventLoop anotherLoop;                                                                  // 这里应该是会使得程序失效的。
}

void threadFunc(){
  printf("threadFunc(): pid = %d, tid = %d\n", getpid(), CurrentThread::tid());
  // 创建每个EventLoop对象的时候都最好进行下面两步断言。
  assert(EventLoop::getEventLoopOfCurrentThread() == NULL);
  EventLoop loop;                                                                             // 子线程又创建一个EventLoop对象。
  assert(EventLoop::getEventLoopOfCurrentThread() == &loop);
  loop.runAfter(1.0, callback);
  loop.loop();
}

int main()
{
  printf("main(): pid = %d, tid = %d\n", getpid(), CurrentThread::tid());       // 查看主线程的id。
  assert(EventLoop::getEventLoopOfCurrentThread() == NULL);               // 断言当前线程是否有 EventLoop 对象。
  EventLoop loop;                                                                               // 如果没有则可以创建一个EventLoop对象，每个线程最多有一个该对象。
  assert(EventLoop::getEventLoopOfCurrentThread() == &loop);               // 断言当前线程EventLoop对象是否是现在的这个。

  Thread thread(threadFunc);                                                                // 创建一个子线程。
  thread.start();                                                                                   // 启动线程。
  // 但是好像还没有注册事件。
  loop.loop();                                                                                      // 并且启动主线程的 loop ，接受连接。
}
