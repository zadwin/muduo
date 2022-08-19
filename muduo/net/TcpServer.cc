// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/TcpServer.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Acceptor.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "muduo/net/SocketsOps.h"

#include <stdio.h>  // snprintf

using namespace muduo;
using namespace muduo::net;

TcpServer::TcpServer(EventLoop* loop,
                     const InetAddress& listenAddr,
                     const string& nameArg,
                     Option option)
  : loop_(CHECK_NOTNULL(loop)),  // 检查loop不是一个空指针。
    ipPort_(listenAddr.toIpPort()),      // 端口号。
    name_(nameArg),
    acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),  // 开始建立监听的套接字。
    threadPool_(new EventLoopThreadPool(loop, name_)),  // 还是主的eventloop。
    connectionCallback_(defaultConnectionCallback),         // 用户的回调函数。
    messageCallback_(defaultMessageCallback),
    nextConnId_(1)
{
  // 设置回调函数。_1对应的是scoket文件描述符，_2对应的是对等方的地址（InetAddress）。
  acceptor_->setNewConnectionCallback(
      std::bind(&TcpServer::newConnection, this, _1, _2));
}

TcpServer::~TcpServer()
{
  loop_->assertInLoopThread();
  LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";

  for (auto& item : connections_)
  {
    TcpConnectionPtr conn(item.second);
    item.second.reset();
    conn->getLoop()->runInLoop(
      std::bind(&TcpConnection::connectDestroyed, conn));
  }
}

void TcpServer::setThreadNum(int numThreads)
{
  assert(0 <= numThreads);
  threadPool_->setThreadNum(numThreads);
}

void TcpServer::start(){
  if (started_.getAndSet(1) == 0){    // 如果没有启动则执行下面的操作。
    threadPool_->start(threadInitCallback_);  // 这里就相当于是启动了这么多个ThreadEventLoop。
    assert(!acceptor_->listening());  // 如果没有处于监听的状态。
    loop_->runInLoop(
        std::bind(&Acceptor::listen, get_pointer(acceptor_)));  // 返回它的原生指针。
  }
}
// 调用TcpServer注册的newConnection函数。参数要求，对等方的地址和文件描述符。
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr){
  loop_->assertInLoopThread();
  EventLoop* ioLoop = threadPool_->getNextLoop();         // 选择一个loop来处理该TCP连接。
  char buf[64];
  snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
  ++nextConnId_;
  string connName = name_ + buf;

  LOG_INFO << "TcpServer::newConnection [" << name_
           << "] - new connection [" << connName
           << "] from " << peerAddr.toIpPort();
  InetAddress localAddr(sockets::getLocalAddr(sockfd));
  // FIXME poll with zero timeout to double confirm the new connection
  // FIXME use make_shared if necessary，生成了一个新的连接，以及它是由那个eventloop对象所管理。
  TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));
  connections_[connName] = conn;  // 通过map管理所有的TCP连接。
  // 设置connection的回调函数。
  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  // 关闭连接的处理方式，因为肯定是调用连接的关闭函数。
  conn->setCloseCallback(
      std::bind(&TcpServer::removeConnection, this, _1)); // FIXME: unsafe
  ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));   // 这里就表示连接已经建立了。
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn){
  // FIXME: unsafe
  loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{ // 发生移除关系。
  loop_->assertInLoopThread();
  LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
           << "] - connection " << conn->name();
  size_t n = connections_.erase(conn->name());                  // 抹除在connections中的channel。
  (void)n;
  assert(n == 1);
  EventLoop* ioLoop = conn->getLoop();                          // 获得conn的loop对象。
  ioLoop->queueInLoop(
      std::bind(&TcpConnection::connectDestroyed, conn));   // 交由loop处理销毁对象。
}

