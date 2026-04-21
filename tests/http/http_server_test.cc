#include "http/http_server.h"

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <thread>

namespace http {
namespace {

// 一个简易的 TCP 客户端工具函数，用于发送请求并读取响应
std::string sendLocalRequest(int port, const std::string& request_str) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) return "";

  sockaddr_in serv_addr{};
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

  if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    close(sock);
    return "";
  }

  // 发送请求报文
  send(sock, request_str.c_str(), request_str.length(), 0);

  // 接收响应报文
  char buffer[4096] = {0};
  std::string response;
  int bytes_read;
  while ((bytes_read = read(sock, buffer, sizeof(buffer) - 1)) > 0) {
    buffer[bytes_read] = '\0';
    response += buffer;
    if (bytes_read < sizeof(buffer) - 1) break;  // 简单假设读完了
  }

  close(sock);
  return response;
}

TEST(HttpServerTest, StartHandleAndStop) {
  int test_port = 8081;  // 使用一个不常用的端口避免冲突
  HttpServer server(test_port);

  // 注册一个测试路由
  server.addHandler("/test", "GET", [](const HttpRequest& req) {
    return HttpResponse(200, "Server is alive");
  });

  // 在后台线程启动服务器
  std::thread server_thread([&server]() { server.run(); });

  // 给服务器 100ms 的时间去 bind 和 listen
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // 模拟浏览器发送真实的 HTTP 请求
  std::string raw_req = "GET /test HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
  std::string response = sendLocalRequest(test_port, raw_req);

  // 验证服务器响应了正确的数据
  EXPECT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
  EXPECT_NE(response.find("Server is alive"), std::string::npos);

  // 测试 404
  std::string not_found_req = "GET /not_exist HTTP/1.1\r\n\r\n";
  std::string response_404 = sendLocalRequest(test_port, not_found_req);
  EXPECT_NE(response_404.find("HTTP/1.1 404 Not Found"), std::string::npos);

  // 优雅停止服务器并等待后台线程汇合
  server.stop();
  if (server_thread.joinable()) {
    server_thread.join();
  }

  // 如果能执行到这里，说明 stop() 成功打破了 accept 阻塞，服务器安全退出了
  SUCCEED();
}

}  // namespace
}  // namespace http