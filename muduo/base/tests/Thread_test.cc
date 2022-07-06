#include "muduo/base/Thread.h"
#include "muduo/base/CurrentThread.h"

#include <string>
#include <stdio.h>
#include <unistd.h>

void mysleep(int seconds)
{
  timespec t = { seconds, 0 };
  // nanosleep()函数会导致当前的线程将暂停执行,直到rqtp参数所指定的时间间隔。或者在指定时间间隔内有信号传递到当前线程，
  //   将引起当前线程调用信号捕获函数或终止该线程。
  nanosleep(&t, NULL);
}

void threadFunc()
{
  printf("tid=%d\n", muduo::CurrentThread::tid());
}

void threadFunc2(int x)
{
  printf("tid=%d, x=%d\n", muduo::CurrentThread::tid(), x);
}

void threadFunc3()
{
  printf("tid=%d\n", muduo::CurrentThread::tid());
  mysleep(1);
}

class Foo
{
 public:
  explicit Foo(double x)   // 这个就表示必须要显示调用。
    : x_(x)
  {
  }

  void memberFunc()
  { // 如果我们取这个函数的地址，那它就是一个普通的函数，因此不同通过何种方式调用都需要将this或是实例对象传递过去。
    printf("tid=%d, Foo::x_=%f\n", muduo::CurrentThread::tid(), x_);
  }

  void memberFunc2(const std::string& text)
  {
    printf("tid=%d, Foo::x_=%f, text=%s\n", muduo::CurrentThread::tid(), x_, text.c_str());
  }

 private:
  double x_;
};

int main()
{
  // 获取当前线程的 tid。
  // 1、getpid是获取当前进程的真实id。2、CurrentThread::tid是为了获取当前线程的真实id（方法为  syscall + cache）。
  // 因为当前只有一个线程——主线程，因此它和进程的真实id是一样的。
  // 提问：为什么这里不用新建对象，而可以直接获取当前线程的tid，主要就是因为在CurrentThread类中，声明了thread类型的变量，而不是一个类成员，因此每个线程都有一份。
  printf("pid=%d, tid=%d\n", ::getpid(), muduo::CurrentThread::tid());
  // 创建一个线程类，这个时候要要传递一个回调函数，这个函数的形式，由类规定，是无参数的，可以通过boost库指定。
  muduo::Thread t1(threadFunc); // 创建一个线程类，包含线程的启动等等。
  t1.start();
  printf("t1.tid=%d\n", t1.tid());
  t1.join(); // 这个是对于主线程来说的，它会等待子线程结束然后执行下面的代码。

  muduo::Thread t2(std::bind(threadFunc2, 42),
                   "thread for free function with argument");
  t2.start();
  printf("t2.tid=%d\n", t2.tid());
  t2.join();

  Foo foo(87.53);
  muduo::Thread t3(std::bind(&Foo::memberFunc, &foo),
                   "thread for member function without argument");
  t3.start();
  t3.join();
  //  C++11 的std::ref函数就是为了解决在线程的创建中等过程的值拷贝问题。
  // 在这种方式下，即使memberFunc2是通过引用的方式接收，也会发生值拷贝，因此需要用ref。
  muduo::Thread t4(std::bind(&Foo::memberFunc2, std::ref(foo), std::string("Shuo Chen")));
  t4.start();
  t4.join();

  {
    muduo::Thread t5(threadFunc3);
    t5.start();
    // t5 may destruct eariler than thread creation.
  }
  mysleep(2);
  {
    muduo::Thread t6(threadFunc3);
    t6.start();
    mysleep(2);
    // t6 destruct later than thread creation.
  }
  sleep(2);
  printf("number of created threads %d\n", muduo::Thread::numCreated());
}
