#pragma once
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace utils {

class ThreadPool {
 public:
  explicit ThreadPool(size_t num_threads);
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  /**
   * 1. 通过变长参数模板、拆包，以及 invoke_result，能够自动适配各种函数类型
   * 2. Args&&... 是万能引用，根据传入的是左值/右值，用左值引用/右值引用对其绑定
   * 3. 万能引用一般与完美转发组合，确保右值引用在传递时仍以“右值”的身份传递
   * 4. std::invoke_result 根据可调用对象类型和参数类型推断返回值类型
   * 5. auto 不如 decltype，会丢掉引用和 const 属性，一般要搭配 ->，这里不得不
   */
  template <typename F, typename... Args>  // 变长参数模板
  auto enqueue(F&& f, Args&&... args)      // 拆包
      -> std::future<typename std::invoke_result<F, Args...>::type>;

 private:
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;  // 无参数无返回的“任务”函数

  std::mutex queue_mutex_;
  std::condition_variable condition_;
  bool stop_;
};

template <typename F, typename... Args>          // 变长参数模板
auto ThreadPool::enqueue(F&& f, Args&&... args)  // 拆包
    -> std::future<typename std::invoke_result<F, Args...>::type> {
  using return_type = typename std::invoke_result<F, Args...>::type;

  // std::bind 可以将函数对象与参数绑定为 void() 闭包，返回值通过 future 传递
  auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...));

  std::future<return_type> res = task->get_future();
  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (stop_) throw std::runtime_error("enqueue on stopped ThreadPool");

    tasks_.emplace([task]() { (*task)(); });  // Lambda 闭包内持有共享指针
  }
  condition_.notify_one();
  return res;  // 返回future，在主线程中通过 f->get() 阻塞等待返回结果
}
}  // namespace utils