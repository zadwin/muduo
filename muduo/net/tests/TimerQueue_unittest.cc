#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/base/Thread.h"

#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

int cnt = 0;
// 用一个 EventLoop 去处理定时器事件。
EventLoop* g_loop;

void printTid(){
  printf("pid = %d, tid = %d\n", getpid(), CurrentThread::tid());
  printf("now %s\n", Timestamp::now().toString().c_str());
}

void print(const char* msg){
  printf("msg %s %s\n", Timestamp::now().toString().c_str(), msg);
  if (++cnt == 20){
    g_loop->quit();
  }
}

void cancel(TimerId timer){
  g_loop->cancel(timer);
  printf("cancelled at %s\n", Timestamp::now().toString().c_str());
}

int main()
{
  printTid();                   // 打印线程 Id
  sleep(1);
  {
    EventLoop loop;
    g_loop = &loop;       // 全局的 eventloop

    print("main");
    // 相当于就是我在 1 s 之后就要调用一下这个函数。它会生成一个timer定时器，会反应在EventLoop中，会被监听到。
    // 在Timer事件中，都是用函数注册的方式。
    loop.runAfter(1, std::bind(print, "once1"));
    loop.runAfter(1.5, std::bind(print, "once1.5"));
    loop.runAfter(2.5, std::bind(print, "once2.5"));
    loop.runAfter(3.5, std::bind(print, "once3.5"));
    TimerId t45 = loop.runAfter(4.5, std::bind(print, "once4.5"));
    // 在任务还没执行的时候，就注销 t45 这个定时器任务。
    loop.runAfter(4.2, std::bind(cancel, t45));
    loop.runAfter(4.8, std::bind(cancel, t45));
    loop.runEvery(2, std::bind(print, "every2"));     // 每隔 2 s 运行一下该函数。
    TimerId t3 = loop.runEvery(3, std::bind(print, "every3"));
    // 在这个时候注销任务。
    loop.runAfter(9.001, std::bind(cancel, t3));

    loop.loop();      // 启动 loop
    print("main loop exits");
  }
  sleep(1);
  {
    // loopthread。
    EventLoopThread loopThread;
    EventLoop* loop = loopThread.startLoop(); // loop函数。
    loop->runAfter(2, printTid);                       // 这里就不是在 eventloop 线程内运行了。
    sleep(3);
    print("thread loop exits");
  }
}
