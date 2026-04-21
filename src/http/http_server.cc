#include "http/http_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

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
      thread_pool_(std::thread::hardware_concurrency()),  // 最大并发线程数
      static_dir_("./static") {
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    throw std::runtime_error("Failed to create socket");
  }

  int opt = 1;  // 设置端口复用
  if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    throw std::runtime_error("Failed to set socket options");
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;  // 监听所有网卡
  address.sin_port = htons(port_);

  if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
    throw std::runtime_error("Failed to bind to port");
  }

  if (listen(server_fd_, 10) < 0) {
    throw std::runtime_error("Failed to listen");
  }
}

HttpServer::~HttpServer() {
  stop();
  if (server_fd_ >= 0) {
    close(server_fd_);
  }
}

void HttpServer::addHandler(const std::string& path, const std::string& method,
                            RequestHandler handler) {
  handlers_[path][method] = std::move(handler);
}

void HttpServer::run() {
  is_running_ = true;
  LOG_INFO << "Server started, listening on port " << port_;

  while (is_running_) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    // 阻塞等待客户端连接
    int client_fd =
        accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);

    if (client_fd < 0) {
      if (!is_running_) {
        LOG_INFO << "Server listener shutting down gracefully.";
        break;
      }
      LOG_ERROR << "Failed to accept connection: " << strerror(errno);
      continue;
    }

    LOG_DEBUG << "Accepted connection from " << inet_ntoa(client_addr.sin_addr)
              << ":" << ntohs(client_addr.sin_port) << " (fd: " << client_fd
              << ")";

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
      // 1. 先用 shutdown 彻底切断底层的收发，瞬间唤醒沉睡的 accept
      shutdown(server_fd_, SHUT_RDWR);

      // 2. 然后再用 close 释放文件描述符资源
      close(server_fd_);
      server_fd_ = -1;
    }
  }
}

void HttpServer::handleClient(int client_fd) {
  char buffer[4096];
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
        response = method_it->second(request);
      } else {
        LOG_WARN << "Method not allowed: " << request.method << " for "
                 << request.path;
        response = HttpResponse(405, "{\"message\":\"Method not allowed\"}");
        response.headers["Content-Type"] = "application/json";
      }
    } else if (request.path == "/" ||
               request.path.find('.') != std::string::npos) {
      std::string path = request.path == "/" ? "/index.html" : request.path;
      std::string full_path = static_dir_ + path;
      LOG_DEBUG << "Serving static file: " << full_path;
      serveStaticFile(full_path, response);
    } else {
      LOG_WARN << "Route not found: " << request.path;
      response = HttpResponse(404, "{\"message\":\"Not found\"}");
      response.headers["Content-Type"] = "application/json";
    }

    // 跨域支持
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

  // 关闭连接
  close(client_fd);
}

void HttpServer::serveStaticFile(const std::string& path,
                                 HttpResponse& response) {
  struct stat sb;
  if (stat(path.c_str(), &sb) != 0) {
    LOG_WARN << "Static file not found: " << path;
    response = HttpResponse(404, "<h1>404 File Not Found</h1>");
    return;
  }

  std::string ext = path.substr(path.find_last_of('.') + 1);
  const std::unordered_map<std::string, std::string> mimeTypes = {
      {"html", "text/html"},
      {"css", "text/css"},
      {"js", "application/javascript"},
      {"json", "application/json"},
      {"png", "image/png"},
      {"jpg", "image/jpeg"},
      {"gif", "image/gif"},
      {"ico", "image/x-icon"}};

  auto mimeType = mimeTypes.find(ext);
  if (mimeType != mimeTypes.end()) {
    response.headers["Content-Type"] = mimeType->second;
  } else {
    response.headers["Content-Type"] = "application/octet-stream";
  }

  std::ifstream file(path, std::ios::binary);
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
