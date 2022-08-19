// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_CHANNEL_H
#define MUDUO_NET_CHANNEL_H

#include "muduo/base/noncopyable.h"
#include "muduo/base/Timestamp.h"

#include <functional>
#include <memory>

namespace muduo
{
namespace net
{

class EventLoop;

///
/// A selectable I/O channel.
///
/// This class doesn't own the file descriptor.
/// The file descriptor could be
/// a socket, an eventfd, a timerfd, or a signalfd  ，可以监听这几种事件（文件描述符）。
class Channel : noncopyable                                 // 都是不可拷贝的。
{         // 不同的事件都注册了不同的回调函数。
 public:
  typedef std::function<void()> EventCallback;                          // 事件的回调处理。
  typedef std::function<void(Timestamp)> ReadEventCallback;   // 读事件的回调处理。

  //  一个 EventLoop 包含多个 channel。但是一个channel只能在一个EventLoop中处理。
  // 其中的 fd 就表示对应的事件，对应的文件描述符。
  Channel(EventLoop* loop, int fd);
  ~Channel();

  void handleEvent(Timestamp receiveTime);
  // 各种事件的回调函数。
  void setReadCallback(ReadEventCallback cb){ readCallback_ = std::move(cb); }
  void setWriteCallback(EventCallback cb){ writeCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback cb){ closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback cb){ errorCallback_ = std::move(cb); }

  /// Tie this channel to the owner object managed by shared_ptr,
  /// prevent the owner object being destroyed in handleEvent.
  void tie(const std::shared_ptr<void>&);

  int fd() const { return fd_; }
  int events() const { return events_; }
  void set_revents(int revt) { revents_ = revt; } // used by pollers
  // int revents() const { return revents_; }
  bool isNoneEvent() const { return events_ == kNoneEvent; }
  // 关注读的事件，调用update会调用EventLoop的updateChannel，这样就可以改变监听事件的类型。
  // 下面同理。
  void enableReading() { events_ |= kReadEvent; update(); }
  void disableReading() { events_ &= ~kReadEvent; update(); }
  void enableWriting() { events_ |= kWriteEvent; update(); }
  void disableWriting() { events_ &= ~kWriteEvent; update(); }
  void disableAll() { events_ = kNoneEvent; update(); } // 不关注事件。
  bool isWriting() const { return events_ & kWriteEvent; }
  bool isReading() const { return events_ & kReadEvent; }

  // for Poller
  int index() { return index_; }
  void set_index(int idx) { index_ = idx; }

  // for debug，将事件转化为字符串，更加便于调试。
  string reventsToString() const;
  string eventsToString() const;

  void doNotLogHup() { logHup_ = false; }

  EventLoop* ownerLoop() { return loop_; }
  void remove();      // remove函数，调用前确保调用了disabledall函数。

 private:
  static string eventsToString(int fd, int ev);

  void update();
  void handleEventWithGuard(Timestamp receiveTime);

  // 三种事件的常量。用于去判断当前返回的事件类型。
  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop* loop_;     // 所属EventLoop
  const int  fd_;           //  文件描述符，但是不负责关闭该文件描述符。
  int        events_;        // 关注的事件
  int        revents_; // it's the received event types of epoll or poll   实际返回的事件，实际返回的事件可能是和关注的有一定的出入。
  int        index_; // used by Poller.         表示poll的事件数组中的序号，就是我们关注的那个数组。小于 0 表示还没添加过去。
  bool       logHup_;

  std::weak_ptr<void> tie_;  // 这是一个弱引用。
  bool tied_;
  bool eventHandling_;    // 是否处于处理事件中。
  bool addedToLoop_;
  // 下面是几个函数的回调函数。
  ReadEventCallback readCallback_;      // 和其他的相比，多了一个事件戳。
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_CHANNEL_H
