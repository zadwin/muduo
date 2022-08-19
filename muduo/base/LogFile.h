// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_LOGFILE_H
#define MUDUO_BASE_LOGFILE_H

#include "muduo/base/Mutex.h"
#include "muduo/base/Types.h"

#include <memory>

namespace muduo
{

namespace FileUtil
{
class AppendFile;  // 应是文件的类。
}

class LogFile : noncopyable
{
 public:
  LogFile(const string& basename,
          off_t rollSize,
          bool threadSafe = true, // 线程安全默认是ture。
          int flushInterval = 3,
          int checkEveryN = 1024);
  ~LogFile();

  void append(const char* logline, int len);
  void flush(); // 清空缓冲区。
  bool rollFile(); // 滚动日志。

 private:
  void append_unlocked(const char* logline, int len); // 不加锁添加。
  // 获取日志文件的名称。
  static string getLogFileName(const string& basename, time_t* now);

  const string basename_;    // 日志文件的basename
  const off_t rollSize_;         //  日志文件的滚动大小
  const int flushInterval_;     // 日志写入的间隔时间，也就是会间隔一段时间才会写入到文件。
  const int checkEveryN_;

  int count_;                       // 一个计数文件，配合checkEveryN_使用。
  // 想要实现写入文件的线程安全。
  std::unique_ptr<MutexLock> mutex_; // 互斥量，配合一个智能指针。
  time_t startOfPeriod_;       // 开始记录日志的时间。会调整至0点。
  time_t lastRoll_;                // 上一次滚动日志文件时间。
  time_t lastFlush_;              // 上一次日志写入文件时间。
  std::unique_ptr<FileUtil::AppendFile> file_; // 也就是操作文件的类。

  const static int kRollPerSeconds_ = 60*60*24; // 一天的时间。
};

}  // namespace muduo
#endif  // MUDUO_BASE_LOGFILE_H
