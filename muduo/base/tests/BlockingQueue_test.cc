#include "muduo/base/BlockingQueue.h"
#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Thread.h"

#include <memory>
#include <string>
#include <vector>
#include <stdio.h>
#include <unistd.h>

class Test
{
 public:
  Test(int numThreads)
    : latch_(numThreads) // 这里有一个门闩倒计时，5个线程的数量。
  {
    for (int i = 0; i < numThreads; ++i)
    {
      char name[32];
      snprintf(name, sizeof name, "work thread %d", i);
      // 构建线程。
      threads_.emplace_back(new muduo::Thread(
            // 这可能就是在模仿真实类的传参方式。
            std::bind(&Test::threadFunc, this), muduo::string(name)));
    }
    for (auto& thr : threads_)  // 所有线程的启动。
    {
      thr->start();
    }
  }

  void run(int times)
  {   // 主线程去添加产品。
    printf("waiting for count down latch\n");
    latch_.wait();    // 等待所有线程启动。
    printf("all threads started\n");
    for (int i = 0; i < times; ++i)
    {
      char buf[32];
      snprintf(buf, sizeof buf, "hello %d", i);
      queue_.put(buf);   // 添加产品。
      printf("tid=%d, put data = %s, size = %zd\n", muduo::CurrentThread::tid(), buf, queue_.size());
    }
  }

  void joinAll()
  {
    for (size_t i = 0; i < threads_.size(); ++i)
    {
      queue_.put("stop");   // 添加了线程数量的 "stop"。
    }

    for (auto& thr : threads_)
    {
      thr->join();
    }
  }

 private:

  void threadFunc()
  {
    printf("tid=%d, %s started\n",
           muduo::CurrentThread::tid(),
           muduo::CurrentThread::name());

    latch_.countDown();  // 这里进行时间的减少。
    bool running = true;
    while (running)
    {
      std::string d(queue_.take());   // 消费资源
      printf("tid=%d, get data = %s, size = %zd\n", muduo::CurrentThread::tid(), d.c_str(), queue_.size());
      running = (d != "stop");  // 直达获取到产品 "stop"，才推出循环。
    }

    printf("tid=%d, %s stopped\n",
           muduo::CurrentThread::tid(),
           muduo::CurrentThread::name());
  }

  muduo::BlockingQueue<std::string> queue_;  // 资源队列
  muduo::CountDownLatch latch_;
  // 线程的数组。
  std::vector<std::unique_ptr<muduo::Thread>> threads_;
};

void testMove()
{
  muduo::BlockingQueue<std::unique_ptr<int>> queue;
  queue.put(std::unique_ptr<int>(new int(42)));
  std::unique_ptr<int> x = queue.take();
  printf("took %d\n", *x);
  *x = 123;
  queue.put(std::move(x));
  std::unique_ptr<int> y = queue.take();
  printf("took %d\n", *y);
}

int main()
{
  // 都是首先获取线程和进程的id。
  printf("pid=%d, tid=%d\n", ::getpid(), muduo::CurrentThread::tid());
  Test t(5);
  t.run(100);
  t.joinAll();

  testMove();

  printf("number of created threads %d\n", muduo::Thread::numCreated());
}
