/**
 * @file Timestamp_unittest.cc
 * @author your name (you@domain.com)
 * @brief   可以用bench来测试机器获取时间的间隔，间隔越大说明机器越慢。
 * @version 0.1
 * @date 2022-04-19
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "muduo/base/Timestamp.h"
#include <vector>
#include <stdio.h>

using muduo::Timestamp;

void passByConstReference(const Timestamp& x)
{
  printf("%s\n", x.toString().c_str());
}

void passByValue(Timestamp x)
{
  printf("%s\n", x.toString().c_str());
}

void benchmark()
{
  const int kNumber = 1000*1000; // const常量名称的前面增加一个k，google建议的写法。

  std::vector<Timestamp> stamps;
  stamps.reserve(kNumber); // 预留了一个很大的空间。
  for (int i = 0; i < kNumber; ++i)
  { // 目的就是为了测试，gettimeofday所花费的时间。
    stamps.push_back(Timestamp::now());
  }
  printf("%s\n", stamps.front().toString().c_str());
  printf("%s\n", stamps.back().toString().c_str());
  printf("%f\n", timeDifference(stamps.back(), stamps.front()));  // 计算第一个时间和最后一个时间的差值。

  int increments[100] = { 0 };
  int64_t start = stamps.front().microSecondsSinceEpoch();
  for (int i = 1; i < kNumber; ++i)
  {
    int64_t next = stamps[i].microSecondsSinceEpoch();
    int64_t inc = next - start;
    start = next;
    if (inc < 0)
    {
      printf("reverse!\n");
    }
    else if (inc < 100)
    {
      ++increments[inc];
    }
    else
    {
      printf("big gap %d\n", static_cast<int>(inc));
    }
  }

  for (int i = 0; i < 100; ++i)
  {
    printf("%2d: %d\n", i, increments[i]);
  }
}

int main()
{
  Timestamp now(Timestamp::now()); // 构造一个时间戳对象，now是一个静态成员函数，返回的是当前时间，然后又拷贝构造给now。
  printf("%s\n", now.toString().c_str());
  passByValue(now);
  passByConstReference(now);
  benchmark(); // 这是一个基准函数，参照函数。
}

