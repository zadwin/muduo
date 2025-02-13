// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADLOCALSINGLETON_H
#define MUDUO_BASE_THREADLOCALSINGLETON_H

#include "muduo/base/noncopyable.h"

#include <assert.h>
#include <pthread.h>

namespace muduo
{
// 大部分为静态成员。模版类。
template<typename T>
class ThreadLocalSingleton : noncopyable
{
 public:  // 不能new的。
  ThreadLocalSingleton() = delete;
  ~ThreadLocalSingleton() = delete;
  // 获取对象。
  static T& instance()
  {
    if (!t_value_)
    {
      t_value_ = new T();
      deleter_.set(t_value_);     // 为了能够是的指针所指向的对象最后能够销毁。
    }
    return *t_value_;
  }

  static T* pointer()
  {
    return t_value_;
  }

 private:
  static void destructor(void* obj)
  {
    assert(obj == t_value_);
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    T_must_be_complete_type dummy; (void) dummy;
    delete t_value_;
    t_value_ = 0;
  }

  class Deleter
  {
   public:
    Deleter()
    {
      // 创建pkey。并且指定删除器。
      pthread_key_create(&pkey_, &ThreadLocalSingleton::destructor);
    }

    ~Deleter()
    {
      pthread_key_delete(pkey_);
    }

    void set(T* newObj)
    { // 将对象和pkey进行绑定。
      assert(pthread_getspecific(pkey_) == NULL);
      pthread_setspecific(pkey_, newObj);   // 与线程pkey_进行绑定。这样最后才能调用销毁函数。
    }
    // POSIX线程库的线程特定数据（TSD）来实现的。
    pthread_key_t pkey_;  // 这里还什么了 pkey ，每个线程都有。
  };
  // 指针式POD类型。主要就是去和 pkey_  相绑定。
  static __thread T* t_value_;  // 线程本地数据。   // 每个线程都本地拥有，并且是static类型的。这个类是不需要new的。
  static Deleter deleter_;  // 这些都是静态的。利用这个类来删除new的T对象
};

template<typename T>
__thread T* ThreadLocalSingleton<T>::t_value_ = 0;

template<typename T>
typename ThreadLocalSingleton<T>::Deleter ThreadLocalSingleton<T>::deleter_;

}  // namespace muduo
#endif  // MUDUO_BASE_THREADLOCALSINGLETON_H
