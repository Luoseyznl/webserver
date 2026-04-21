#include "utils/timer.h"

namespace utils {

Timer::Timer() : running_(false) {}

Timer::~Timer() { stop(); }

void Timer::addOnceTask(std::chrono::milliseconds delay,
                        std::function<void()> callback) {
  addPeriodicTask(delay, std::chrono::milliseconds(0), std::move(callback));
}

void Timer::addPeriodicTask(std::chrono::milliseconds delay,
                            std::chrono::milliseconds period,
                            std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto execution_time = std::chrono::steady_clock::now() + delay;
  bool is_periodic = (period.count() > 0);

  tasks_.push(Task(execution_time, std::move(callback), is_periodic, period));

  cv_.notify_one();
}

void Timer::start() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!running_) {
    running_ = true;
    timer_thread_ = std::thread([this] { processTimerTasks(); });
  }
}

void Timer::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    cv_.notify_one();
  }
  if (timer_thread_.joinable()) {
    timer_thread_.join();
  }
}

void Timer::processTimerTasks() {
  while (true) {
    std::unique_lock<std::mutex> lock(mutex_);

    cv_.wait(lock, [this] { return !running_ || !tasks_.empty(); });

    if (!running_) break;

    auto now = std::chrono::steady_clock::now();
    auto earliest_time = tasks_.top().execution_time;

    if (earliest_time > now) {
      cv_.wait_until(lock, earliest_time);
      continue;  // 醒来后重新检查堆顶任务
    }

    Task task = tasks_.top();  // priority_queue 的 top 是 const 引用，只能拷贝
    tasks_.pop();

    if (task.is_periodic) {
      task.execution_time = std::chrono::steady_clock::now() + task.period;
      tasks_.push(task);  // 周期性任务，计算下一次时间并重新塞回队列
    }

    // ！！！解锁后执行任务！！！
    lock.unlock();

    if (task.callback) {
      task.callback();
    }
  }
}

};  // namespace utils

