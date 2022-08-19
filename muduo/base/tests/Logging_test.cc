#include "muduo/base/Logging.h"
#include "muduo/base/LogFile.h"
#include "muduo/base/ThreadPool.h"
#include "muduo/base/TimeZone.h"

#include <stdio.h>
#include <unistd.h>

int g_total;
FILE* g_file;
std::unique_ptr<muduo::LogFile> g_logFile;   // 空的全局文件。

void dummyOutput(const char* msg, int len)
{
  g_total += len;
  if (g_file)
  {
    fwrite(msg, 1, len, g_file);
  }
  else if (g_logFile)
  {
    g_logFile->append(msg, len);
  }
}

void bench(const char* type)
{
  muduo::Logger::setOutput(dummyOutput);  // 这里有默认的输出。
  muduo::Timestamp start(muduo::Timestamp::now());
  g_total = 0;

  int n = 1000*1000;
  const bool kLongLog = false;
  muduo::string empty = " ";
  muduo::string longStr(3000, 'X');
  longStr += " ";
  for (int i = 0; i < n; ++i)
  {
    LOG_INFO << "Hello 0123456789" << " abcdefghijklmnopqrstuvwxyz"
             << (kLongLog ? longStr : empty)
             << i;
  }
  muduo::Timestamp end(muduo::Timestamp::now());
  double seconds = timeDifference(end, start);
  printf("%12s: %f seconds, %d bytes, %10.2f msg/s, %.2f MiB/s\n",
         type, seconds, g_total, n / seconds, g_total / seconds / (1024 * 1024));
}

void logInThread()
{  // 每个线程去打印日志，默认的输出是标准输出。
  LOG_INFO << "logInThread";
  usleep(1000);
}

int main()
{
  // 获取父进程的pid。
  getppid(); // for ltrace and strace

  muduo::ThreadPool pool("pool");  // 一个线程池。
  pool.start(5);                              // 运行5个线程。
  pool.run(logInThread);
  pool.run(logInThread);
  pool.run(logInThread);
  pool.run(logInThread);
  pool.run(logInThread);
  // 以下是主线程的输出。
  LOG_TRACE << "trace";
  LOG_DEBUG << "debug";
  LOG_INFO << "Hello";
  LOG_WARN << "World";
  LOG_ERROR << "Error";
  LOG_INFO << sizeof(muduo::Logger);
  LOG_INFO << sizeof(muduo::LogStream);
  LOG_INFO << sizeof(muduo::Fmt);
  LOG_INFO << sizeof(muduo::LogStream::Buffer);

  sleep(1);
  bench("nop");  // 这是一个性能测试程序。

  char buffer[64*1024];

  g_file = fopen("/dev/null", "w");  // 这个时候就定位到了文件当中。
  setbuffer(g_file, buffer, sizeof buffer);
  bench("/dev/null");  // 测试写入到这个文件的性能。
  fclose(g_file);

  g_file = fopen("/tmp/log", "w");  // 打开的是另外的文件。
  setbuffer(g_file, buffer, sizeof buffer);
  bench("/tmp/log");
  fclose(g_file);

  g_file = NULL;  // 用新的文件类来测试，一个是线程安全的，一个是非线程安全的。
  g_logFile.reset(new muduo::LogFile("test_log_st", 500*1000*1000, false));
  bench("test_log_st");

  g_logFile.reset(new muduo::LogFile("test_log_mt", 500*1000*1000, true));
  bench("test_log_mt");
  g_logFile.reset();

  {
  g_file = stdout;  // 重新定位到标准输出。
  sleep(1);
  muduo::TimeZone beijing(8*3600, "CST");
  muduo::Logger::setTimeZone(beijing);
  LOG_TRACE << "trace CST";
  LOG_DEBUG << "debug CST";
  LOG_INFO << "Hello CST";
  LOG_WARN << "World CST";
  LOG_ERROR << "Error CST";

  sleep(1);
  muduo::TimeZone newyork("/usr/share/zoneinfo/America/New_York");
  muduo::Logger::setTimeZone(newyork);
  LOG_TRACE << "trace NYT";
  LOG_DEBUG << "debug NYT";
  LOG_INFO << "Hello NYT";
  LOG_WARN << "World NYT";
  LOG_ERROR << "Error NYT";
  g_file = NULL;
  }
  bench("timezone nop");
}
