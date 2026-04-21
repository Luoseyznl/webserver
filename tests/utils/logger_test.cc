#include "utils/logger.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <vector>

#include "utils/thread_pool.h"

namespace utils {

TEST(LoggerTest, HeavyDutyAsyncLogging) {
  const std::string test_log_dir = "gtest_logs";

  if (std::filesystem::exists(test_log_dir)) {
    std::filesystem::remove_all(test_log_dir);
  }

  LogConfig config;
  config.log_dir = test_log_dir;
  config.max_file_size = 256 * 1024;
  config.max_files = 4;
  config.async_mode = true;

  Logger::init(config);
  Logger::setGlobalLevel(LogLevel::INFO);

  auto start_time = std::chrono::steady_clock::now();

  const int num_threads = 8;
  const int logs_per_thread = 10000;

  {
    ThreadPool pool(num_threads);
    std::vector<std::future<void>> results;

    for (int i = 0; i < num_threads; ++i) {
      results.emplace_back(pool.enqueue([i, logs_per_thread]() {
        for (int j = 0; j < logs_per_thread; ++j) {
          LOG_INFO << "Task " << i << " iteration " << j
                   << " - padding buffer data";
        }
      }));
    }

    // 阻塞等待
    for (auto& f : results) {
      f.get();
    }
  }

  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  EXPECT_LT(duration.count(), 1000) << "Logger 性能未达标";

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  int file_count = 0;
  long long total_size = 0;

  for (const auto& entry : std::filesystem::directory_iterator(test_log_dir)) {
    if (entry.path().extension() == ".log") {
      file_count++;
      total_size += std::filesystem::file_size(entry.path());
    }
  }

  EXPECT_LE(file_count, config.max_files)
      << "文件数超过 max_files: " << file_count;

  // 边界断言：确保真的生成了文件
  EXPECT_GT(file_count, 0) << "日志未写入磁盘！";
  EXPECT_GT(total_size, 0) << "日志未写入文件！";
}

}  // namespace utils
