// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/Logging.h"
#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"

#include <sstream>

#include <poll.h>

using namespace muduo;
using namespace muduo::net;

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop* loop, int fd__)
  : loop_(loop),
    fd_(fd__),
    events_(0),
    revents_(0),
    index_(-1),
    logHup_(true),
    tied_(false),
    eventHandling_(false),
    addedToLoop_(false){
}

Channel::~Channel(){
  assert(!eventHandling_);
  assert(!addedToLoop_);
  if (loop_->isInLoopThread()){
    assert(!loop_->hasChannel(this));
  }
}

void Channel::tie(const std::shared_ptr<void>& obj){
  tie_ = obj;
  tied_ = true;
}

void Channel::update(){
  addedToLoop_ = true;
  loop_->updateChannel(this); // 将该事件添加或者修改。
}
// 调用这个函数之前，确保调用了disableAll。
void Channel::remove(){
  assert(isNoneEvent());
  addedToLoop_ = false;
  loop_->removeChannel(this);// 这里只是将channel从evnetLoop中移除，并不将负责channel的生存周期。
}
// 当事件到来会调用这个函数。
void Channel::handleEvent(Timestamp receiveTime){
  std::shared_ptr<void> guard;
  if (tied_){
    guard = tie_.lock();
    if (guard){
      handleEventWithGuard(receiveTime);
    }
  }else{
    handleEventWithGuard(receiveTime);
  }
}

void Channel::handleEventWithGuard(Timestamp receiveTime){
  eventHandling_ = true;    // 正在处理。
  LOG_TRACE << reventsToString();     // 通知是什么事件发生了。
  // 然后判断返回的事件，采取相应的措施进行处理。
  // pollhup 只在输出事件中产生，因此同时要确认没有pollin事件。
  if ((revents_ & POLLHUP) && !(revents_ & POLLIN)){
    if (logHup_){ // 这里是警告信息。
      LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLHUP";    // 日志处理。
    }
    if (closeCallback_) closeCallback_(); // 如果被挂断了。
  }
  // pollnval：表示这是一个没有打开或者不合法的文件描述符。因此只是记录一下日志。
  if (revents_ & POLLNVAL){
    LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLNVAL";
  }
  // pollerr: 表示这是错误的事件，调用错误的处理函数。
  if (revents_ & (POLLERR | POLLNVAL)){
    if (errorCallback_) errorCallback_();
  }
  // POLLIN | POLLPRI：这两个都是刻度事件。
  // pollrdhup ： 对等方关闭连接会接收到这样一个事件，这表示是一个可读的事件。
  // 疑问：这里所有的事件都是调用readcallback来处理吗。
  if (revents_ & (POLLIN | POLLPRI | POLLRDHUP)){
    if (readCallback_) readCallback_(receiveTime);
  }
  // pollout ： 这表示的是一个可写的事件。
  if (revents_ & POLLOUT){
    if (writeCallback_) writeCallback_();
  }
  eventHandling_ = false;       // 表示这个处理完成。
}

string Channel::reventsToString() const{
  return eventsToString(fd_, revents_);
}

string Channel::eventsToString() const{
  return eventsToString(fd_, events_);
}
// 用来将事件转化成字符串。
string Channel::eventsToString(int fd, int ev){
  std::ostringstream oss;
  oss << fd << ": ";
  if (ev & POLLIN)
    oss << "IN ";
  if (ev & POLLPRI)
    oss << "PRI ";
  if (ev & POLLOUT)
    oss << "OUT ";
  if (ev & POLLHUP)
    oss << "HUP ";
  if (ev & POLLRDHUP)
    oss << "RDHUP ";
  if (ev & POLLERR)
    oss << "ERR ";
  if (ev & POLLNVAL)
    oss << "NVAL ";

  return oss.str();
}
