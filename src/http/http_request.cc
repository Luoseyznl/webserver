#include "http/http_request.h"

#include <cstdio>
#include <sstream>

namespace http {

HttpRequest HttpRequest::parse(const std::string& request_str) {
  HttpRequest request;
  if (request_str.empty()) return request;
  size_t header_end = request_str.find("\r\n\r\n");  // Header 与 Body 分界符
  if (header_end == std::string::npos) {
    return request;  // 报文不完整
  }

  std::string header_part = request_str.substr(0, header_end);
  request.body = request_str.substr(header_end + 4);

  std::istringstream iss(header_part);  // Header 是纯文本
  std::string line;

  // 1. 请求行：POST /login?name=gew HTTP/1.1
  if (std::getline(iss, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    std::istringstream request_line(line);
    std::string version;
    request_line >> request.method >> request.path >> version;
    request.parseQueryParams();  // 2. 查询参数：/login?name=gew
  } else {
    return request;
  }

  // 3. 请求头
  while (std::getline(iss, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) continue;

    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
      std::string key = line.substr(0, colon_pos);

      size_t value_start = colon_pos + 1;
      while (value_start < line.length() && line[value_start] == ' ') {
        value_start++;  // 跳过空格
      }

      std::string value = line.substr(value_start);

      request.headers[key] = value;
    }
  }

  return request;
}

//  查询参数：/login?name=gew&passwd=123
void HttpRequest::parseQueryParams() {
  size_t pos = path.find('?');
  if (pos != std::string::npos) {
    std::string query_string = path.substr(pos + 1);
    path = path.substr(0, pos);

    size_t start = 0;
    size_t end;
    while ((end = query_string.find('&', start)) != std::string::npos) {
      parseParam(query_string.substr(start, end - start));
      start = end + 1;
    }
    parseParam(query_string.substr(start));
  }
}

// 解析键值对 name=gew
void HttpRequest::parseParam(const std::string& param) {
  size_t equals_pos = param.find('=');
  if (equals_pos != std::string::npos) {
    std::string key = param.substr(0, equals_pos);
    std::string value = param.substr(equals_pos + 1);
    query_params[urlDecode(key)] = urlDecode(value);  // URL 解码
  }
}

std::string HttpRequest::urlDecode(const std::string& encoded) {
  std::string decoded;
  for (size_t i = 0; i < encoded.length(); ++i) {
    if (encoded[i] == '%' && i + 2 < encoded.length()) {
      int value;  // 将 %XX 转换成 16 进制数字再转回字符
      std::sscanf(encoded.substr(i + 1, 2).c_str(), "%x", &value);
      decoded += static_cast<char>(value);
      i += 2;
    } else if (encoded[i] == '+') {  // 老式表单会将 + 作为空格
      decoded += ' ';
    } else {
      decoded += encoded[i];
    }
  }
  return decoded;
}
}  // namespace http
