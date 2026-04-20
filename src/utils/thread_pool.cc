#include "utils/thread_pool.h"

namespace utils {
ThreadPool::ThreadPool(size_t num_threads) : stop_(false) {
  for (size_t i = 0; i < num_threads; ++i) {
    workers_.emplace_back([this] {
      while (true) {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(queue_mutex_);
          condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });

          // 优雅关闭：线程池停止，并且任务队列为空时结束
          if (stop_ && tasks_.empty()) {
            return;
          }

          task = std::move(tasks_.front());
          tasks_.pop();
        }
        // 防御性编程
        if (task) {
          task();
        }
      }
    });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    stop_ = true;
  }
  condition_.notify_all();  // 惊群
  for (std::thread& worker : workers_) {
    worker.join();
  }
}
}  // namespace utils
