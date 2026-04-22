#include "http/http_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "utils/logger.h"

namespace http {

HttpServer::HttpServer(int port)
    : port_(port),
      is_running_(false),
      server_fd_(-1),
      thread_pool_(std::thread::hardware_concurrency()),
      static_dir_("./static") {
  // 1. 创建 server_fd_: IPv4, TCP, 0（自动推导协议类型）
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    throw std::runtime_error("Failed to create socket");
  }

  // 2. 配置 server_fd_: 仅开启端口复用（跳过冷却时间），配置值为 1
  int opt = 1;  //
  if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    throw std::runtime_error("Failed to set socket options");
  }

  sockaddr_in address{};                 // IPv4 地址结构体（in 表示 internet）
  address.sin_family = AF_INET;          // 类型为 AF_INET
  address.sin_addr.s_addr = INADDR_ANY;  // 监听所有网卡 IP（0.0.0.0）
  address.sin_port = htons(port_);       // 设置端口号并转为网络字节序（大端序）

  // 3. 将 server_fd_ 与 address 绑定
  if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
    throw std::runtime_error("Failed to bind to port");
  }

  // 4. 切换 server_fd_ 为监听状态，将内核建立的连接存入全连接队列中（大小为10）
  if (listen(server_fd_, 10) < 0) {
    throw std::runtime_error("Failed to listen");
  }
}

HttpServer::~HttpServer() {
  stop();                 // 1. 停止业务逻辑
  if (server_fd_ >= 0) {  // 2. 资源回收
    close(server_fd_);
  }
}

void HttpServer::addHandler(const std::string& path, const std::string& method,
                            RequestHandler handler) {
  handlers_[path][method] = std::move(handler);
}

// 二段式启动 Two-phase Startup
void HttpServer::run() {
  is_running_ = true;
  LOG_INFO << "Server started, listening on port " << port_;

  while (is_running_) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    // 1. 从 server_fd_ 的全连接队列中阻塞等待 client_fd
    int client_fd =
        accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);

    if (client_fd < 0) {
      if (!is_running_) {
        LOG_INFO << "Server listener shutting down gracefully.";
        break;
      }
      // errno 故障诊断错误码
      LOG_ERROR << "Failed to accept connection: " << strerror(errno);
      continue;
    }

    LOG_DEBUG << "Accepted connection from " << inet_ntoa(client_addr.sin_addr)
              << ":" << ntohs(client_addr.sin_port) << " (fd: " << client_fd
              << ")";  // 身份登记

    // 2. 由后端的 ThreadPool 去并发处理 client_fd
    thread_pool_.enqueue([this, client_fd]() {
      handleClient(client_fd);
      return 0;
    });
  }
}

void HttpServer::stop() {
  if (is_running_) {
    LOG_INFO << "Stopping HttpServer...";
    is_running_ = false;
    if (server_fd_ >= 0) {
      shutdown(server_fd_, SHUT_RDWR);  // 切断底层网络协议栈，并中断 accept
      close(server_fd_);  // 减少内核中的文件描述符的引用计数并释放文件描述符
      server_fd_ = -1;    // 避免悬空引用
    }
  }
}

void HttpServer::handleClient(int client_fd) {
  char buffer[4096];  // 阻塞等待 client_fd 写入并保持末尾 \0
  ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

  if (bytes_read > 0) {
    buffer[bytes_read] = '\0';

    HttpRequest request = HttpRequest::parse(buffer);
    HttpResponse response;

    LOG_INFO << "Request: [" << request.method << "] " << request.path;

    auto path_it = handlers_.find(request.path);
    if (path_it != handlers_.end()) {
      auto method_it = path_it->second.find(request.method);
      if (method_it != path_it->second.end()) {
        response = method_it->second(request);  // std::pair<,>::iterator
      } else {  // method 错误，返回 405 及响应体（JSON 结构化数据）
        LOG_WARN << "Method not allowed: " << request.method << " for "
                 << request.path;
        response = HttpResponse(405, "{\"message\":\"Method not allowed\"}");
        response.headers["Content-Type"] = "application/json";
      }
    } else if (request.path == "/" ||  // 静态资源请求：首页(/) / file.xxx
               request.path.find('.') != std::string::npos) {
      std::string path = request.path == "/" ? "/index.html" : request.path;
      std::string full_path = static_dir_ + path;
      LOG_DEBUG << "Serving static file: " << full_path;
      serveStaticFile(full_path, response);
    } else {  // 业务路径错误
      LOG_WARN << "Route not found: " << request.path;
      response = HttpResponse(404, "{\"message\":\"Not found\"}");
      response.headers["Content-Type"] = "application/json";
    }

    // 允许来自不同域名的访问（跨域访问）
    response.headers["Access-Control-Allow-Origin"] = "*";

    std::string response_str = response.toString();
    ssize_t total_bytes_written = 0;
    const char* data = response_str.c_str();
    size_t remaining = response_str.length();

    // 循环发送，确保大文件和长字符串不被截断
    while (remaining > 0) {
      ssize_t bytes_written =
          write(client_fd, data + total_bytes_written, remaining);
      if (bytes_written < 0) {
        if (errno == EINTR) continue;
        LOG_ERROR << "Failed to send response to fd " << client_fd << ": "
                  << strerror(errno);
        break;
      }
      total_bytes_written += bytes_written;
      remaining -= bytes_written;
    }
  } else if (bytes_read < 0) {
    LOG_ERROR << "Failed to read from client fd " << client_fd << ": "
              << strerror(errno);
  }

  close(client_fd);
}

void HttpServer::serveStaticFile(const std::string& path,
                                 HttpResponse& response) {
  struct stat sb;  // Linux 系统调用，查找文件信息
  if (stat(path.c_str(), &sb) != 0) {
    LOG_WARN << "Static file not found: " << path;
    response = HttpResponse(404, "<h1>404 File Not Found</h1>");
    return;
  }

  std::string ext = path.substr(path.find_last_of('.') + 1);
  const std::unordered_map<std::string, std::string> mimeTypes = {
      {"html", "text/html"},  // MIME 类型字典
      {"css", "text/css"},          {"js", "application/javascript"},
      {"json", "application/json"}, {"png", "image/png"},
      {"jpg", "image/jpeg"},        {"gif", "image/gif"},
      {"ico", "image/x-icon"}};

  auto mimeType = mimeTypes.find(ext);
  if (mimeType != mimeTypes.end()) {
    response.headers["Content-Type"] = mimeType->second;
  } else {
    response.headers["Content-Type"] = "application/octet-stream";
  }

  std::ifstream file(path, std::ios::binary);  // 将资源文件以二进制形式传送
  if (!file) {
    LOG_ERROR << "Failed to open static file for reading: " << path;
    response = HttpResponse(500, "Internal Server Error");
    return;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  response.status_code = 200;
  response.body = buffer.str();
}

}  // namespace http
