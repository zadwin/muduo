#include "muduo/base/BlockingQueue.h"
#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Logging.h"
#include "muduo/base/Thread.h"
#include "muduo/base/Timestamp.h"

#include <map>
#include <string>
#include <vector>
#include <stdio.h>
#include <unistd.h>

bool g_verbose = false;

// Many threads, one queue.
// 用于度量时间的类。
class Bench
{
 public:
  Bench(int numThreads)
    : latch_(numThreads)
  {
    threads_.reserve(numThreads);
    for (int i = 0; i < numThreads; ++i)
    { // 创建线程。用来消费产品，主线程用来生产产品。
      char name[32];
      snprintf(name, sizeof name, "work thread %d", i);
      threads_.emplace_back(new muduo::Thread(
            std::bind(&Bench::threadFunc, this), muduo::string(name)));
    }
    for (auto& thr : threads_)
    {
      thr->start();
    }
  }

  void run(int times)
  {
    printf("waiting for count down latch\n");
    latch_.wait();   // 等待线程的启动。
    LOG_INFO << threads_.size() << " threads started";   // 日志文件。
    int64_t total_delay = 0;
    for (int i = 0; i < times; ++i)
    {  // 主线程生产产品。生产的就是时间。
      muduo::Timestamp now(muduo::Timestamp::now());
      queue_.put(now);
      total_delay += delay_queue_.take();
    }
    printf("Average delay: %.3fus\n", static_cast<double>(total_delay) / times);
  }

  void joinAll()
  {
    for (size_t i = 0; i < threads_.size(); ++i)
    {
      queue_.put(muduo::Timestamp::invalid());
    }

    for (auto& thr : threads_)
    {
      thr->join();
    }
    LOG_INFO << threads_.size() << " threads stopped";
  }

 private:

  void threadFunc()
  {  // 消费者程序。
    if (g_verbose) {
    printf("tid=%d, %s started\n",
           muduo::CurrentThread::tid(),
           muduo::CurrentThread::name());
    }

    std::map<int, int> delays;
    latch_.countDown();
    bool running = true;
    while (running)
    {
      muduo::Timestamp t(queue_.take());
      muduo::Timestamp now(muduo::Timestamp::now());
      // 实际上也就是统计生产产品到消费产品的时间差。
      if (t.valid())
      {
        int delay = static_cast<int>(timeDifference(now, t) * 1000000);
        // printf("tid=%d, latency = %d us\n",
        //        muduo::CurrentThread::tid(), delay);
        ++delays[delay];
        delay_queue_.put(delay);
      }
      running = t.valid();  // 需要一个非法的时间才会跳出循环。
    }

    if (g_verbose)
    {
      printf("tid=%d, %s stopped\n",
             muduo::CurrentThread::tid(),
             muduo::CurrentThread::name());
      for (const auto& delay : delays)
      {  // 输出时间差的计数。
        printf("tid = %d, delay = %d, count = %d\n",
               muduo::CurrentThread::tid(),
               delay.first, delay.second);
      }
    }
  }

  muduo::BlockingQueue<muduo::Timestamp> queue_;
  muduo::BlockingQueue<int> delay_queue_;
  muduo::CountDownLatch latch_;
  std::vector<std::unique_ptr<muduo::Thread>> threads_;
};

// 用于度量时间。
int main(int argc, char* argv[])
{
  int threads = argc > 1 ? atoi(argv[1]) : 1;  // 确定线程的数量是多少。

  Bench t(threads);
  t.run(100000);
  t.joinAll();  // 加入非法的时间。
}
