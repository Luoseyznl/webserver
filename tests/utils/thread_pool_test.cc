#include "utils/thread_pool.h"

#include <gtest/gtest.h>

#include <chrono>
#include <map>
#include <random>

// 匿名空间：保护辅助函数不外泄
namespace {
const long long MOD = 1e9 + 7;
long long fibonacci(int n) {
  if (n <= 1) return n % MOD;
  return (fibonacci(n - 1) + fibonacci(n - 2)) % MOD;
}
}  // namespace

// 镜像命名空间
namespace utils {
// TEST(TestCaseName, TestName)
TEST(ThreadPoolTest, HeavyDutyWorkload) {
  ThreadPool pool(16);
  std::vector<std::future<long long>> results;

  auto start_time = std::chrono::steady_clock::now();

  for (int i = 0; i < 40; ++i) {
    results.emplace_back(pool.enqueue([]() { return fibonacci(35); }));
  }

  long long total_results = 0;
  for (auto& f : results) {
    total_results += f.get();
  }

  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  EXPECT_LT(duration.count(), 1000) << "性能未达标！" << std::endl;
}
}  // namespace utils
