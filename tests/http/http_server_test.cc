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

  send(sock, request_str.c_str(), request_str.length(), 0);

  char buffer[4096] = {0};
  std::string response;
  int bytes_read;
  while ((bytes_read = read(sock, buffer, sizeof(buffer) - 1)) > 0) {
    buffer[bytes_read] = '\0';
    response += buffer;
    if (bytes_read < sizeof(buffer) - 1) break;
  }

  close(sock);
  return response;
}

TEST(HttpServerTest, StartHandleAndStop) {
  int test_port = 8081;
  HttpServer server(test_port);

  server.addHandler("/test", "GET", [](const HttpRequest& req) {
    return HttpResponse(200, "Server is alive");
  });

  std::thread server_thread([&server]() { server.run(); });

  // 给服务器 100ms 的时间去 bind 和 listen
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::string raw_req = "GET /test HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
  std::string response = sendLocalRequest(test_port, raw_req);
  EXPECT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
  EXPECT_NE(response.find("Server is alive"), std::string::npos);

  std::string not_found_req = "GET /not_exist HTTP/1.1\r\n\r\n";
  std::string response_404 = sendLocalRequest(test_port, not_found_req);
  EXPECT_NE(response_404.find("HTTP/1.1 404 Not Found"), std::string::npos);

  server.stop();
  if (server_thread.joinable()) {
    server_thread.join();
  }

  SUCCEED();
}

}  // namespace
}  // namespace http