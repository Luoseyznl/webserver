#pragma once

#include <string>
#include <unordered_map>

namespace http {

/**
 * @brief HTTP 响应报文 DTO (Data Transfer Object)
 * 将结构化数据转换为符合 HTTP/1.1 标准的字符串流
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