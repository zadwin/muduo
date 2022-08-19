// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include "muduo/net/Buffer.h"

#include "muduo/net/SocketsOps.h"

#include <errno.h>
#include <sys/uio.h>

using namespace muduo;
using namespace muduo::net;

const char Buffer::kCRLF[] = "\r\n";

const size_t Buffer::kCheapPrepend;
const size_t Buffer::kInitialSize;

// 结合栈上空间，避免内存使用过大，提高内存使用率。
// 如果有10k个连接，每个连接就分配64k的缓冲区的话，将招用640m内存。
// 而大多数时候，这些缓冲区的使用率很低。
ssize_t Buffer::readFd(int fd, int* savedErrno){
  // saved an ioctl()/FIONREAD call to tell how much to read
  char extrabuf[65536];     // 准备一个足够大的缓冲区，减少系统调用。主要是防止buffer不够大。
  struct iovec vec[2];
  const size_t writable = writableBytes();
  // 第一块缓冲区。
  vec[0].iov_base = begin()+writerIndex_;
  vec[0].iov_len = writable;
  // 第二块缓冲区。
  vec[1].iov_base = extrabuf;
  vec[1].iov_len = sizeof extrabuf;
  // when there is enough space in this buffer, don't read into extrabuf.
  // when extrabuf is used, we read 128k-1 bytes at most.
  const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
  const ssize_t n = sockets::readv(fd, vec, iovcnt);      // 使用readv来提高效率。
  if (n < 0){
    *savedErrno = errno;
  }else if (implicit_cast<size_t>(n) <= writable){    // 第一块缓冲区足够容纳，则直接写。
    writerIndex_ += n;
  }else{  // 当前缓冲区，不够容纳，因为数据被接收到extrabuf，将其append至buffuer中。
    writerIndex_ = buffer_.size();
    append(extrabuf, n - writable);
  }
  // if (n == writable + sizeof extrabuf)
  // {
  //   goto line_30;
  // }
  return n;
}

