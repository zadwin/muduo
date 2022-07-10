#include "muduo/base/Singleton.h"
#include "muduo/base/CurrentThread.h"
#include "muduo/base/ThreadLocal.h"
#include "muduo/base/Thread.h"

#include <stdio.h>
#include <unistd.h>

class Test : muduo::noncopyable
{
 public:
  Test()
  {
    printf("tid=%d, constructing %p\n", muduo::CurrentThread::tid(), this);
  }

  ~Test()
  {
    printf("tid=%d, destructing %p %s\n", muduo::CurrentThread::tid(), this, name_.c_str());
  }

  const muduo::string& name() const { return name_; }
  void setName(const muduo::string& n) { name_ = n; }

 private:
  muduo::string name_;
};

// 线程本地最后还是通过pkey_实现线程的私有化。也就是从头到尾我只有这么一个  ThreadLocal<Test>的对象在整个程序中。
#define STL muduo::Singleton<muduo::ThreadLocal<Test> >::instance().value()

void print()
{
  printf("tid=%d, %p name=%s\n",
         muduo::CurrentThread::tid(),
         &STL,  // 输出的这个就相当于是Test对象。
         STL.name().c_str());
}

void threadFunc(const char* changeTo)
{
  print();
  STL.setName(changeTo);
  sleep(1);
  print();
}

int main()
{
  //  其实也就相当于是 ThreadLocal 是单例的。但是意义是什么？——直接单例Test对象不久可以吗。
  STL.setName("main one");
  muduo::Thread t1(std::bind(threadFunc, "thread1"));
  muduo::Thread t2(std::bind(threadFunc, "thread2"));
  t1.start();
  t2.start();
  t1.join();
  print();
  t2.join();
  pthread_exit(0);  // 这里应该也就是主线程结束。
}
