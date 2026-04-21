#pragma once

#include <string>
#include <unordered_map>

namespace http {

/**
 * @brief HTTP 请求报文 DTO
 * * * 职责：
 * 1. 接收来自 Socket 的原始字符串流
 * 2. 将字符串解析为结构化数据
 * 3. 解析 URL 中的 Query Parameters（例如 ?id=1&name=gew）。
 */
class HttpRequest {
 public:
  std::string method;  // 请求方法："GET", "POST"
  std::string path;    // 路由路径：path -> (method -> requestHandler)
  std::string body;    // 请求体：JSON, 表单数据
  std::unordered_map<std::string, std::string> headers;       // 请求头键值对
  std::unordered_map<std::string, std::string> query_params;  // URL 参数键值对

  HttpRequest() = default;

  static HttpRequest parse(const std::string& request_str);

 private:
  void parseQueryParams();
  void parseParam(const std::string& param);
  std::string urlDecode(const std::string& encoded);
};

}  // namespace http