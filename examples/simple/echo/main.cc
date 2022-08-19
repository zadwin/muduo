#include "examples/simple/echo/echo.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

#include <unistd.h>

// using namespace muduo;
// using namespace muduo::net;

int main()
{
  LOG_INFO << "pid = " << getpid();
  muduo::net::EventLoop loop;                     // 首先定义了一个事件循环的对象。one loop per thread + thread pool 模型。
  muduo::net::InetAddress listenAddr(2007);  // 初始化一个地址对象。
  EchoServer server(&loop, listenAddr);        // 通过传递loop对象，也就是对reactor对象的封装，然后就是一个EchoServer对象。
  server.start();                                         // 比如包含了创建套接字、绑定、监听等等。
  loop.loop();                                            // 里面就包含了等待事件的到来，然后进行相应的回调处理。
}

