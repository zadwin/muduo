// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_SINGLETON_H
#define MUDUO_BASE_SINGLETON_H

#include "muduo/base/noncopyable.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h> // atexit

namespace muduo
{

namespace detail
{
// This doesn't detect inherited member functions!
// http://stackoverflow.com/questions/1966362/sfinae-to-check-for-inherited-member-functions
template<typename T>
struct has_no_destroy
{
  template <typename C> static char test(decltype(&C::no_destroy));
  template <typename C> static int32_t test(...);
  const static bool value = sizeof(test<T>(0)) == 1;
};
}  // namespace detail

template<typename T>
class Singleton : noncopyable
{
 public:
  Singleton() = delete;
  ~Singleton() = delete;

  static T& instance()
  {
    // 因此也就只会new出一个对象。这个函数是线程安全的。
    pthread_once(&ponce_, &Singleton::init);   // 保证函数只被调用一次。
    assert(value_ != NULL);
    return *value_;
  }

 private:
  static void init()
  {
    value_ = new T();  // 这个函数只会执行一次，因此也就只会产生一个对象。
    if (!detail::has_no_destroy<T>::value)
    {
      ::atexit(destroy);  // 相当于是登记销毁函数。整个程序结束的时候会自动销毁。
    }
  }

  static void destroy()
  {
    // 因为我们下面要销毁对象value_，因此必须保证该对象是完全类型，比如不能是指针。
    // 如果不满足会在编译期间报错，这是一个数组。
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    T_must_be_complete_type dummy; (void) dummy;

    delete value_;
    value_ = NULL;
  }

 private:
  static pthread_once_t ponce_;  // 这个变量是用了判断某个函数是否还能够执行。
  static T*             value_;
};

template<typename T>
// 采用这个初始值保证初始化函数只会执行一次。
pthread_once_t Singleton<T>::ponce_ = PTHREAD_ONCE_INIT;

template<typename T>
T* Singleton<T>::value_ = NULL;

}  // namespace muduo

#endif  // MUDUO_BASE_SINGLETON_H
