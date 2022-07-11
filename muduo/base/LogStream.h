// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_LOGSTREAM_H
#define MUDUO_BASE_LOGSTREAM_H

#include "muduo/base/noncopyable.h"
#include "muduo/base/StringPiece.h"
#include "muduo/base/Types.h"
#include <assert.h>
#include <string.h> // memcpy

namespace muduo
{

namespace detail
{

const int kSmallBuffer = 4000;
const int kLargeBuffer = 4000*1000;

template<int SIZE>
class FixedBuffer : noncopyable
{
 public:
  FixedBuffer()
    : cur_(data_)
  {
    setCookie(cookieStart);
  }

  ~FixedBuffer()
  {
    setCookie(cookieEnd);
  }
  // 添加数据
  void append(const char* /*restrict*/ buf, size_t len)
  {
    // FIXME: append partially   可用空间够了就添加进去。
    // google：用implicit_cast可以实现 一种 static_cast 或者 const_cast在向上转型时的安全版本。
    if (implicit_cast<size_t>(avail()) > len)
    {
      memcpy(cur_, buf, len);  // 对应内通的填充。
      cur_ += len;
    }
  }

  const char* data() const { return data_; }
  int length() const { return static_cast<int>(cur_ - data_); }  // 当前的长度。

  // write to data_ directly
  char* current() { return cur_; }
  int avail() const { return static_cast<int>(end() - cur_); }   // 当前可用的空间。
  void add(size_t len) { cur_ += len; }

  void reset() { cur_ = data_; }   // 重置缓冲区。只需要更改位置，并不要清空。
  // sizeof data_：这里的data_也可以是一个表达式。
  void bzero() { memZero(data_, sizeof data_); } // 就是将数据都设置为0.

  // for used by GDB
  const char* debugString();      // 将缓冲区后面加一个\0
  void setCookie(void (*cookie)()) { cookie_ = cookie; }
  // for used by unit test
  string toString() const { return string(data_, length()); }  // 构造一个string对象。
  StringPiece toStringPiece() const { return StringPiece(data_, length()); }

 private:
  const char* end() const { return data_ + sizeof data_; }  // 返回缓冲池的末尾位置。
  // Must be outline function for cookies.
  static void cookieStart();
  static void cookieEnd();

  void (*cookie_)();
  char data_[SIZE]; // 这个是一个缓冲池，用来缓存数据。它一直指向首地址。
  char* cur_;         // 这个是用来指向还没有填充的位置。
};

}  // namespace detail

// 采用这个对象所继承的一些东西，去进行输入。流对象复杂将日志信息写入到内存中。
class LogStream : noncopyable
{
  typedef LogStream self;
 public:
  // k 开头的一般是一个常量，google编程风格。
  typedef detail::FixedBuffer<detail::kSmallBuffer> Buffer;  // 固定的缓存池。

  self& operator<<(bool v)
  {
    buffer_.append(v ? "1" : "0", 1);
    return *this;
  }
  // 重载了这一些操作符。
  self& operator<<(short);
  self& operator<<(unsigned short);
  self& operator<<(int);
  self& operator<<(unsigned int);
  self& operator<<(long);
  self& operator<<(unsigned long);
  self& operator<<(long long);
  self& operator<<(unsigned long long);

  self& operator<<(const void*);

  self& operator<<(float v)
  {
    *this << static_cast<double>(v);
    return *this;
  }
  self& operator<<(double);
  // self& operator<<(long double);

  self& operator<<(char v)
  {
    buffer_.append(&v, 1);   // 这里相当于只是将它append到了一个池子里面。
    return *this;
  }

  // self& operator<<(signed char);
  // self& operator<<(unsigned char);

  self& operator<<(const char* str)
  {
    if (str)
    {
      buffer_.append(str, strlen(str));
    }
    else
    {
      buffer_.append("(null)", 6);
    }
    return *this;
  }

  self& operator<<(const unsigned char* str)
  {
    return operator<<(reinterpret_cast<const char*>(str));
  }

  self& operator<<(const string& v)
  {
    buffer_.append(v.c_str(), v.size());  // 基本都是用append。
    return *this;
  }

  self& operator<<(const StringPiece& v)
  {
    buffer_.append(v.data(), v.size());
    return *this;
  }

  self& operator<<(const Buffer& v)
  {
    *this << v.toStringPiece();
    return *this;
  }

  void append(const char* data, int len) { buffer_.append(data, len); }
  const Buffer& buffer() const { return buffer_; }
  void resetBuffer() { buffer_.reset(); }

 private:
  void staticCheck();

  template<typename T>
  void formatInteger(T);

  Buffer buffer_;   // 包含了一个缓冲池对象。
  static const int kMaxNumericSize = 48;
};

class Fmt // : noncopyable
{
 public:
  template<typename T>   // 这是一个成员模版，用于适应不同的构造函数。
  Fmt(const char* fmt, T val);   // 将整数格式化到buffer中。

  const char* data() const { return buf_; }
  int length() const { return length_; }

 private:
  char buf_[32];
  int length_;
};

// 将 LogStream 格式化到缓存当中。
inline LogStream& operator<<(LogStream& s, const Fmt& fmt)
{
  s.append(fmt.data(), fmt.length());
  return s;
}

// Format quantity n in SI units (k, M, G, T, P, E).
// The returned string is atmost 5 characters long.
// Requires n >= 0
string formatSI(int64_t n);

// Format quantity n in IEC (binary) units (Ki, Mi, Gi, Ti, Pi, Ei).
// The returned string is atmost 6 characters long.
// Requires n >= 0
string formatIEC(int64_t n);

}  // namespace muduo

#endif  // MUDUO_BASE_LOGSTREAM_H
