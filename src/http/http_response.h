#pragma once

#include <string>
#include <unordered_map>

namespace http {

/**
 * @brief HTTP 响应报文 DTO (Data Transfer Object)
 * * 职责：
 * 1. 存储后端逻辑处理后的结果：状态码、响应体、Header
 * 2. 将结构化数据转换为符合 HTTP/1.1 标准的字符串流
 * * 协作：
 * - HttpServer::RequestHandler -> HttpResponse
 */
class HttpResponse {
 public:
  int status_code;
  std::string body;
  std::unordered_map<std::string, std::string> headers;

  HttpResponse(int code = 200, const std::string& body = "");
  std::string toString() const;

 private:
  static std::string getStatusText(int code);
};

}  // namespace http