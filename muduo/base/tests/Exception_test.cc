#include "muduo/base/CurrentThread.h"
#include "muduo/base/Exception.h"
#include <functional> // 这里其实用到的就是stl中bind和function。对函数重新配置。
#include <vector>
#include <stdio.h>

class Bar
{
 public:
 // 该函数多次打印了函数栈的信息，通过不同的函数。
  void test(std::vector<std::string> names = {})
  {
    printf("Stack:\n%s\n", muduo::CurrentThread::stackTrace(true).c_str());
    [] {
      printf("Stack inside lambda:\n%s\n", muduo::CurrentThread::stackTrace(true).c_str());
    }();
    std::function<void()> func([] {
      printf("Stack inside std::function:\n%s\n", muduo::CurrentThread::stackTrace(true).c_str());
    });
    func();

    func = std::bind(&Bar::callback, this);
    func();
    // 可以理解为他会去Exception里面获取调用获取栈信息的函数。
    throw muduo::Exception("oops");
  }

 private:
   void callback()
   {
     printf("Stack inside std::bind:\n%s\n", muduo::CurrentThread::stackTrace(true).c_str());
   }
};

void foo()
{
  Bar b;
  b.test();
}

int main()
{
  try
  {
    foo();
  }
  catch (const muduo::Exception& ex)
  {
    printf("reason: %s\n", ex.what());
    printf("stack trace:\n%s\n", ex.stackTrace());
  }
}
