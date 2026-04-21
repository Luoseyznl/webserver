#include "utils/timer.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

namespace utils {

// 1. 测试定时任务
TEST(TimerTest, OnceTaskAccuracy) {
  Timer timer;
  timer.start();

  std::atomic<bool> task_executed{false};
  auto start_time = std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point exec_time;

  // 启动一个 100ms 的非周期定时任务
  timer.addOnceTask(std::chrono::milliseconds(100), [&]() {
    exec_time = std::chrono::steady_clock::now();
    task_executed = true;
  });

  // 主线程等待 150 ms
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  timer.stop();

  EXPECT_TRUE(task_executed) << "定时任务未执行";
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                      exec_time - start_time)
                      .count();
  EXPECT_GE(duration, 90) << "定时任务执行太早";
  EXPECT_LE(duration, 110) << "定时任务执行太晚";
}

// 2. 测试任务优先级排序
TEST(TimerTest, TaskOrdering) {
  Timer timer;
  timer.start();

  std::vector<int> execution_order;
  std::mutex order_mutex;

  timer.addOnceTask(std::chrono::milliseconds(300), [&]() {
    std::lock_guard<std::mutex> lock(order_mutex);
    execution_order.push_back(300);
  });

  timer.addOnceTask(std::chrono::milliseconds(100), [&]() {
    std::lock_guard<std::mutex> lock(order_mutex);
    execution_order.push_back(100);
  });

  timer.addOnceTask(std::chrono::milliseconds(200), [&]() {
    std::lock_guard<std::mutex> lock(order_mutex);
    execution_order.push_back(200);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(400));
  timer.stop();

  ASSERT_EQ(execution_order.size(), 3) << "优先级任务丢失";
  EXPECT_EQ(execution_order[0], 100);
  EXPECT_EQ(execution_order[1], 200);
  EXPECT_EQ(execution_order[2], 300);
}

// 3. 测试定时周期任务
TEST(TimerTest, PeriodicTaskExecution) {
  Timer timer;
  timer.start();

  std::atomic<int> run_count{0};
  // 首此延迟 50ms，之后每 50ms 执行一次
  timer.addPeriodicTask(std::chrono::milliseconds(50),
                        std::chrono::milliseconds(50), [&]() { run_count++; });

  // 理论上会在 50, 100, 150, 200 时刻各执行一次，共计 4 次
  std::this_thread::sleep_for(std::chrono::milliseconds(225));
  timer.stop();

  EXPECT_GE(run_count.load(), 3) << "周期性任务执行过少";
  EXPECT_LE(run_count.load(), 5) << "周期性任务执行过多";
}
}  // namespace utils
