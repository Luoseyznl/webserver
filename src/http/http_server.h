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
 * 1. 创建 server_fd 监听端口
 * 2. 维护业务路由表：[URL PATH] -> ([METHOD] -> RequestHandler)
 * 3. 通过线程池并发处理 client_fd：读取 HttpRequest、解析、回写 HttpResponse
 */
class HttpServer {
 public:
  using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

  explicit HttpServer(int port);
  ~HttpServer();

  /**
   * @brief 注册路由处理 Routing Handler
   * 1. POST: register, login, create_room, join_room, send_message, logout
   * 2. GET: rooms, users, messages, 静态资源
   * @example
   * addHandler("/login", "POST", [](const HttpRequest& req) {
   * ...
   * return HttpResponse(200, "{\"status\":\"ok\"}");
   */
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

  // 业务路由处理表：Path -> (Method -> Handler)
  std::unordered_map<std::string,
                     std::unordered_map<std::string, RequestHandler>>
      handlers_;

  void handleClient(int client_fd);
  void serveStaticFile(const std::string& path, HttpResponse& response);
};
}  // namespace http
