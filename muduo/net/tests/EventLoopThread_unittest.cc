#include "muduo/net/EventLoopThread.h"
#include "muduo/net/EventLoop.h"
#include "muduo/base/Thread.h"
#include "muduo/base/CountDownLatch.h"

#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

void print(EventLoop* p = NULL){
  printf("print: pid = %d, tid = %d, loop = %p\n",
         getpid(), CurrentThread::tid(), p);
}

void quit(EventLoop* p){
  print(p);
  p->quit();
}

int main(){
  print();
  {
  // 创建了一个线程 EventLoop 实例，但是没有启动它。
  EventLoopThread thr1;  // never start
  }

  {
    // dtor calls quit()
    // 创建线程 EventLoop 实例。
    // 这里类的EventLoop实例什么时候销毁，什么时候停止。会在EventLoopThreqad类销毁destor的时候，停止和销毁EventLoop。
    EventLoopThread thr2;
    EventLoop *loop = thr2.startLoop();      // 启动这个线程IO，其中会有执行loop的函数。
    loop->runInLoop(std::bind(print, loop)); // 在主线程中给IO线程注册执行函数，这里就相当于异步执行了。
    CurrentThread::sleepUsec(500 * 1000);    // 也就是将当前主线程暂停。
  }

  {
  // quit() before dtor
  EventLoopThread thr3;
  EventLoop* loop = thr3.startLoop();
  loop->runInLoop(std::bind(quit, loop));
  CurrentThread::sleepUsec(500 * 1000);
  }
}

