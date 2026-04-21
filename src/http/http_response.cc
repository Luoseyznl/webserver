#include "http/http_response.h"

#include <sstream>

namespace http {

// text/plain: 纯文本模式，不做排版和渲染
// text/html: 启动渲染引擎 Blink，构建可视化 Web 界面
// application/json: 前端会将 JSON 数据 转换成 JavaScript 对象进行逻辑处理
HttpResponse::HttpResponse(int code, const std::string& body_content)
    : status_code(code), body(body_content) {
  headers["Content-Type"] = "text/html; charset=utf-8";  // 默认为 html
  headers["Connection"] = "keep-alive";
}

std::string HttpResponse::toString() const {
  std::ostringstream oss;

  oss << "HTTP/1.1 " << status_code << " " << getStatusText(status_code)
      << "\r\n";
  oss << "Content-Length: " << body.length() << "\r\n";

  for (const auto& [key, value] : headers) {
    if (key != "Content-Length") {
      oss << key << ": " << value << "\r\n";  // 回车换行（html 传统换行符）
    }
  }

  oss << "\r\n";  // \r\n\r\n 是 Header 和 Body 的分界线
  oss << body;
  return oss.str();
}

std::string HttpResponse::getStatusText(int code) {
  switch (code) {
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 301:
      return "Moved Permanently";
    case 302:
      return "Found";
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 409:
      return "Conflict";
    case 500:
      return "Internal Server Error";
    case 502:
      return "Bad Gateway";
    default:
      return "Unknown";
  }
}
}  // namespace http