#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "http/http_request.h"
#include "http/http_response.h"
#include "utils/thread_pool.h"

namespace http {

/**
 * @brief HttpServer
 * * 职责：
 * 1. 监听端口：管理底层的 Server Socket 生命周期
 * 2. 路由分发：维护 Path 到 Handler 的映射表
 * 3. 并发调度：将客户端的 Socket I/O 交给底层的 ThreadPool 处理
 */
class HttpServer {
 public:
  // 业务逻辑：HttpRequest -> HttpResponse
  using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

  explicit HttpServer(int port);
  ~HttpServer();

  // 注册动态路由：登录逻辑、
  void addHandler(const std::string& path, const std::string& method,
                  RequestHandler handler);

  void run();
  void stop();

 private:
  int server_fd_;
  int port_;
  bool is_running_;
  std::string static_dir_;

  utils::ThreadPool thread_pool_;

  std::unordered_map<std::string,
                     std::unordered_map<std::string, RequestHandler>>
      handlers_;  // 业务路由表：Path -> (Method -> Handler)

  void handleClient(int client_fd);
  void serveStaticFile(const std::string& path, HttpResponse& response);
};
}  // namespace http
