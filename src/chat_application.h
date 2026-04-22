#pragma once

#include <memory>
#include <string>

#include "db/database_manager.h"
#include "http/http_server.h"

namespace chat {

class ChatApplication {
 public:
  ChatApplication(const ChatApplication&) = delete;
  ChatApplication& operator=(const ChatApplication&) = delete;

  ChatApplication(int port, const std::string& db_path);
  ~ChatApplication();

  void start();  // 应用层二段式启动
  void stop();

 private:
  void setupRoutes();

  int port_;
  std::unique_ptr<DatabaseManager> db_manager_;
  std::unique_ptr<http::HttpServer> http_server_;
};

}  // namespace chat