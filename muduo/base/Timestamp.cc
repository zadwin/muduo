// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

/**
 * 不同的用户采用不同的解决方案：.cc， .cpp，.cxx以及其它可能的。
 *        今天，在Unix世界之外，它主要是.cpp。Unix似乎.cc更经常使用。
 *
 */

#include "muduo/base/Timestamp.h"

#include <sys/time.h>
#include <stdio.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS  // 主要是为了适应下面这个   inttypes.h 头文件。
#endif

#include <inttypes.h>

using namespace muduo;

/**
 * @brief 这里是编译时候的断言语句。而我们平常用的assert是运行时的断言语句。
 */
static_assert(sizeof(Timestamp) == sizeof(int64_t),
              "Timestamp should be same size as int64_t");

/**
 * @brief 该函数的目的也是将为了将时间转换为字符串类型，但是只要秒和微秒就行。
 *            知识点：
 *                  1、PRId64（是一个系统定义的宏，在inttype.h中定义）:主要是为了更好的适应32位系统（ld）和64位系统（lld）中的int64的表示方式。
 */
string Timestamp::toString() const
{
  char buf[32] = {0};
  int64_t seconds = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
  int64_t microseconds = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;
  // 这个函数和下面的函数一样，也是返回一个字符串。
  // 同样是64位的整数，在32位的系统中打印输出的方式不同，例如 32位系统 为lld，64位系统为ld。
  // 这也是一个跨平台该有的做法。
  snprintf(buf, sizeof(buf), "%" PRId64 ".%06" PRId64 "", seconds, microseconds);
  return buf;
}

string Timestamp::toFormattedString(bool showMicroseconds) const
{
  char buf[64] = {0};
  time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
  struct tm tm_time;  // 内置的结构体。
  // 下面这个函数的  _r  表示是一个线程安全的函数。
  gmtime_r(&seconds, &tm_time); // 将秒数转化为tm的结构体，这样就可以直接使用其中的年月日等信息

  if (showMicroseconds)
  {
    int microseconds = static_cast<int>(microSecondsSinceEpoch_ % kMicroSecondsPerSecond);
    snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
             microseconds);
  }
  else
  {
    snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
  }
  return buf;
}

Timestamp Timestamp::now()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);  // 通过内置的Linux函数获得一个当前时间的结构体。后面的NULL表示的是时区。
  int64_t seconds = tv.tv_sec;
  return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}

