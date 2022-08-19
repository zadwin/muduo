// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/poller/PollPoller.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Types.h"
#include "muduo/net/Channel.h"

#include <assert.h>
#include <errno.h>
#include <poll.h>

using namespace muduo;
using namespace muduo::net;

PollPoller::PollPoller(EventLoop* loop)
  : Poller(loop)
{
}

PollPoller::~PollPoller() = default;

// 对 poll IO多路复用的实现。
Timestamp PollPoller::poll(int timeoutMs, ChannelList* activeChannels){
  // XXX pollfds_ shouldn't change
  // 一旦超时了，那么也就不在等待了，继续执行下面的操作，不能让它一直阻塞，因为需要它处理其他的事务。
  int numEvents = ::poll(&*pollfds_.begin(), pollfds_.size(), timeoutMs); // 这里也是有一个超时时间，可以用于打印日志。
  int savedErrno = errno;
  Timestamp now(Timestamp::now());
  if (numEvents > 0){
    LOG_TRACE << numEvents << " events happened";
    fillActiveChannels(numEvents, activeChannels);                                  // 只将活跃的事件放入 activeChannels 中。
  }else if (numEvents == 0){
    LOG_TRACE << " nothing happened";
  }else{
    if (savedErrno != EINTR){
      errno = savedErrno;
      LOG_SYSERR << "PollPoller::poll()";
    }
  }
  return now;
}
// 遍历这个pollfd向量，找出活跃的事件并且返回回去。
void PollPoller::fillActiveChannels(int numEvents,
                                    ChannelList* activeChannels) const {
  for (PollFdList::const_iterator pfd = pollfds_.begin();
      pfd != pollfds_.end() && numEvents > 0; ++pfd){               // 循环事件列表，找出活跃的事件，并将它放入通道中。
    if (pfd->revents > 0) {
      --numEvents;
      ChannelMap::const_iterator ch = channels_.find(pfd->fd);  // 判断该事件是否在我们需要监听的map中，找也是用fd去寻找channel的。
      assert(ch != channels_.end());
      Channel* channel = ch->second;
      assert(channel->fd() == pfd->fd);
      channel->set_revents(pfd->revents);     // 设置返回的事件类型。
      // pfd->revents = 0;
      activeChannels->push_back(channel);
    }
  }
}
 // 用于注册或者更新事件。
void PollPoller::updateChannel(Channel* channel)
{
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events();
  if (channel->index() < 0)  { // 表示是一个新的通道。
    // a new one, add to pollfds_
    // 用了一个map去存储所有关注的channe对象，主要是可以更快速的查找，不用逐个遍历。
    assert(channels_.find(channel->fd()) == channels_.end());
    struct pollfd pfd;        // 一个 poll 结构体。
    pfd.fd = channel->fd();
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;
    pollfds_.push_back(pfd);
    int idx = static_cast<int>(pollfds_.size())-1;
    channel->set_index(idx);  // 设置它的index。
    channels_[pfd.fd] = channel;
  }else{ // 表示是修改事件的类型。
    // update existing one
    assert(channels_.find(channel->fd()) != channels_.end());
    assert(channels_[channel->fd()] == channel);
    int idx = channel->index();
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
    struct pollfd& pfd = pollfds_[idx];   // 得到对应pollfd结构体。
    assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd() - 1); // pfd.fd == -channel->fd()-1 主要是为了处理不关注的事件。
    // 因为采用是引用的方式，因此只要这里修改我们就能够。后面调用poll函数的时候需要将数组传入里面的事件也就是想要关注的了。
    pfd.fd = channel->fd();
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;
    // 将一个通道暂时更改为不关注事件，但不从poller中一移除该通道。
    if (channel->isNoneEvent()){
      // ignore this pollfd
      // 负数就表示不是一个合法的文件描述符，这样在遍历的时候就可以跳过了。只是为了revmovechannel优化。
      pfd.fd = -channel->fd()-1;  // 只是pfd的文件符不对了，但是channel中还是记录的对的文件描述符。
    }
  }
}
// 这里才是真正的将事件从poll中移除，不关注。
void PollPoller::removeChannel(Channel* channel){
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd();
  assert(channels_.find(channel->fd()) != channels_.end());
  assert(channels_[channel->fd()] == channel);
  assert(channel->isNoneEvent());               // 需要提前将所有事件给关闭。
  int idx = channel->index();                       // 取出在数组中的大小。
  assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
  const struct pollfd& pfd = pollfds_[idx]; (void)pfd;
  assert(pfd.fd == -channel->fd()-1 && pfd.events == channel->events());    // 断言，确定是否统一。
  size_t n = channels_.erase(channel->fd());                                               // 通过key移除。
  assert(n == 1); (void)n;
  if (implicit_cast<size_t>(idx) == pollfds_.size()-1) { // 最后一个移除。
    pollfds_.pop_back();
  }else { // 不是最后一个移除。采用交换的移除方法，这样就能够减少一部分的内存拷贝了。
    int channelAtEnd = pollfds_.back().fd;
    iter_swap(pollfds_.begin()+idx, pollfds_.end()-1);
    if (channelAtEnd < 0) {
    // 特别要注意这里。因为我们之前将目前不关注的事件的文件描述符置为负数了。
    //  我们要存储的一定是正确的，只是在传递给poll的时候设置为负数。
      channelAtEnd = -channelAtEnd-1;
    }
    channels_[channelAtEnd]->set_index(idx);
    pollfds_.pop_back();
  }
}

