#include "http/http_response.h"

#include <gtest/gtest.h>

namespace http {
namespace {

TEST(HttpResponseTest, DefaultConstructor) {
  HttpResponse resp;
  EXPECT_EQ(resp.status_code, 200);
  EXPECT_TRUE(resp.body.empty());

  std::string output = resp.toString();
  EXPECT_NE(output.find("HTTP/1.1 200 OK\r\n"), std::string::npos);
  EXPECT_NE(output.find("Content-Length: 0\r\n"), std::string::npos);
}

TEST(HttpResponseTest, CustomHeadersAndBody) {
  HttpResponse resp(404, "{\"error\": \"not found\"}");
  resp.headers["Content-Type"] = "application/json";
  resp.headers["X-Custom-Header"] = "gew-test";

  std::string output = resp.toString();

  EXPECT_NE(output.find("HTTP/1.1 404 Not Found\r\n"), std::string::npos);
  EXPECT_NE(output.find("Content-Type: application/json\r\n"),
            std::string::npos);
  EXPECT_NE(output.find("X-Custom-Header: gew-test\r\n"), std::string::npos);
  EXPECT_NE(output.find("Content-Length: 22\r\n"), std::string::npos);
  EXPECT_NE(output.find("\r\n\r\n{\"error\": \"not found\"}"),
            std::string::npos);
}

}  // namespace
}  // namespace http