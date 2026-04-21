#include "http/http_request.h"

#include <gtest/gtest.h>

namespace http {
namespace {

TEST(HttpRequestTest, ParseSimpleGet) {
  std::string raw =
      "GET /index.html HTTP/1.1\r\n"
      "Host: localhost:8080\r\n"
      "User-Agent: curl/7.68.0\r\n"
      "\r\n";

  HttpRequest req = HttpRequest::parse(raw);

  EXPECT_EQ(req.method, "GET");
  EXPECT_EQ(req.path, "/index.html");
  EXPECT_EQ(req.headers["Host"], "localhost:8080");
  EXPECT_EQ(req.headers["User-Agent"], "curl/7.68.0");
  EXPECT_TRUE(req.body.empty());
}

TEST(HttpRequestTest, ParsePostWithBody) {
  std::string raw =
      "POST /api/login HTTP/1.1\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: 27\r\n"
      "\r\n"
      "{\"name\":\"gew\",\"age\":25}";

  HttpRequest req = HttpRequest::parse(raw);

  EXPECT_EQ(req.method, "POST");
  EXPECT_EQ(req.path, "/api/login");
  EXPECT_EQ(req.headers["Content-Type"], "application/json");
  EXPECT_EQ(req.body, "{\"name\":\"gew\",\"age\":25}");
}

TEST(HttpRequestTest, ParseQueryParamsAndUrlDecode) {
  std::string raw =
      "GET /search?keyword=c%2B%2B%20server&id=1001 HTTP/1.1\r\n"
      "\r\n";

  HttpRequest req = HttpRequest::parse(raw);

  EXPECT_EQ(req.method, "GET");
  EXPECT_EQ(req.path, "/search");

  ASSERT_EQ(req.query_params.size(), 2);
  EXPECT_EQ(req.query_params["keyword"], "c++ server");
  EXPECT_EQ(req.query_params["id"], "1001");
}

TEST(HttpRequestTest, ParseMalformedRequest) {
  std::string raw = "GET / HTTP/1.1\r\nHost: localhost";
  HttpRequest req = HttpRequest::parse(raw);

  EXPECT_TRUE(req.method.empty());
  EXPECT_TRUE(req.path.empty());
}

}  // namespace
}  // namespace http