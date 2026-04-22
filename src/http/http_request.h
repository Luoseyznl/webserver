#pragma once

#include <string>
#include <unordered_map>

namespace http {

/**
 * @brief HTTP 请求报文 DTO
 * 将 request_str 解析为结构化数据
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