// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_POLLER_POLLPOLLER_H
#define MUDUO_NET_POLLER_POLLPOLLER_H

#include "muduo/net/Poller.h"

#include <vector>

struct pollfd;

namespace muduo
{
namespace net
{
// 认为是一些内部类。
///
/// IO Multiplexing with poll(2).
///
class PollPoller : public Poller
{
 public:
  PollPoller(EventLoop* loop);
  ~PollPoller() override;

  Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;  // 返回活动通道。
  void updateChannel(Channel* channel) override;                                // 更新或者添加通道。
  void removeChannel(Channel* channel) override;                               // 移除通道。

 private:
 // 将活动的事件都返回到activeChannels数组中，这样就省去了每次都遍历的特点。
  void fillActiveChannels(int numEvents,
                          ChannelList* activeChannels) const;

  typedef std::vector<struct pollfd> PollFdList;                                  // 内置poll需要的结构体数组，这里只是声明了一个类型。
  // pollfd结构体：
  //            int fd;
  //            short events; // 表示要告诉操作系统需要监测fd的事件
  //            short revents;// revents域是文件描述符操作结果事件，内核在调用返回时设置这个域。主要是告诉该文件描述符发生了什么事。

  PollFdList pollfds_;                                                                      // 用来存储pollfd的结构体数组。
};

}  // namespace net
}  // namespace muduo
#endif  // MUDUO_NET_POLLER_POLLPOLLER_H
