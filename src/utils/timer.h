#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace utils {

// TODO: Dispatch expired timer callbacks to a thread pool
class Timer {
 public:
  struct Task {
    std::chrono::steady_clock::time_point execution_time;
    std::function<void()> callback;
    bool is_periodic;
    std::chrono::milliseconds period;

    Task(std::chrono::steady_clock::time_point time, std::function<void()> cb,
         bool periodic = false,
         std::chrono::milliseconds p = std::chrono::milliseconds(0))
        : execution_time(time),
          callback(cb),
          is_periodic(periodic),
          period(p) {}

    // 比较函数，用于大顶堆（小顶堆用的比较函数必须是 const）
    bool operator>(const Task& other) const {
      return execution_time > other.execution_time;
    }
  };

  explicit Timer();  // 防御式编程
  ~Timer();

  void addOnceTask(std::chrono::milliseconds delay,
                   std::function<void()> callback);

  void addPeriodicTask(std::chrono::milliseconds delay,
                       std::chrono::milliseconds period,
                       std::function<void()> callback);

  void start();
  void stop();

 private:
  void processTimerTasks();

  std::priority_queue<Task, std::vector<Task>, std::greater<Task>> tasks_;

  std::mutex mutex_;
  std::condition_variable cv_;
  std::thread timer_thread_;
  bool running_;
};

}  // namespace utils

