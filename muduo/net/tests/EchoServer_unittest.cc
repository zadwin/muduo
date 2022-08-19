#include "muduo/net/TcpServer.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Thread.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"

#include <utility>

#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

int numThreads = 0;   // 相当于这里就没有用到 ThreadPool。

class EchoServer
{
 public:
  EchoServer(EventLoop* loop, const InetAddress& listenAddr)
    : loop_(loop),
      server_(loop, listenAddr, "EchoServer")
  {
    // 这里就是连接完成的回调函数。
    server_.setConnectionCallback(
        std::bind(&EchoServer::onConnection, this, _1));
    server_.setMessageCallback(
        std::bind(&EchoServer::onMessage, this, _1, _2, _3));
    server_.setThreadNum(numThreads);
  }
  void start(){
    server_.start();
  }
  // void stop();

 private:
  void onConnection(const TcpConnectionPtr& conn){    // 连接到来的处理方式。
    LOG_TRACE << conn->peerAddress().toIpPort() << " -> "
        << conn->localAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");
    LOG_INFO << conn->getTcpInfoString();  // 还有一个输出关于TcpInfo。
    conn->send("hello\n");
  }
  // 用到的是Buffer类。
  void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
  {
    string msg(buf->retrieveAllAsString());
    LOG_TRACE << conn->name() << " recv " << msg.size() << " bytes at " << time.toString();
    if (msg == "exit\n")
    {
      conn->send("bye\n");
      conn->shutdown(); // 关闭的是套接字的写事件。
    }
    if (msg == "quit\n")
    {
      loop_->quit();      // 这里就相当于是停止了loop。
    }
    conn->send(msg);    // 服务器又将对应的消息给发送回去了。
  }

  EventLoop* loop_;
  TcpServer server_;
};

// 是不是没有设计计算线程池呢？
int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
  LOG_INFO << "sizeof TcpConnection = " << sizeof(TcpConnection);           // TcpConnection 的大小。
  if (argc > 1)
  {
    numThreads = atoi(argv[1]);
  }
  bool ipv6 = argc > 2;
  EventLoop loop;                                   // 一个 EventLoop 实例。
  InetAddress listenAddr(2000, false, ipv6);// 生成一个 网络地址。
  EchoServer server(&loop, listenAddr);     // 服务器。
  server.start();     // 启动监听的服务。
  loop.loop();        // 启动 IO 服务。
}

