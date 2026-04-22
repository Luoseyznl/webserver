#include "chat_application.h"

#include <chrono>
#include <nlohmann/json.hpp>

#include "utils/logger.h"

using json = nlohmann::json;

namespace chat {

static int64_t getCurrentTimeMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

static http::HttpResponse makeJsonResponse(int status_code,
                                           const std::string& body) {
  http::HttpResponse response(status_code, body);
  response.headers["Content-Type"] = "application/json";
  return response;
}

ChatApplication::ChatApplication(int port, const std::string& db_path)
    : port_(port) {
  db_manager_ = std::make_unique<DatabaseManager>(db_path);
  http_server_ = std::make_unique<http::HttpServer>(port_);
}

ChatApplication::~ChatApplication() { stop(); }

void ChatApplication::start() {
  LOG_INFO << "Initializing Chat Application...";

  setupRoutes();  // 挂载路由

  LOG_INFO << "Routes configured. Server is ready on port " << port_;

  http_server_->run();  // 启动引擎（阻塞主线程）
}

void ChatApplication::stop() {
  if (http_server_) {
    http_server_->stop();  // 停止引擎
  }
}

void ChatApplication::setupRoutes() {
  // ==========================================
  // 1. 用户注册 (/register)
  // ==========================================
  http_server_->addHandler(
      "/register", "POST",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          json data = json::parse(request.body);
          if (!data.contains("username") || !data.contains("password")) {
            return makeJsonResponse(
                400, "{\"error\":\"Missing username or password\"}");
          }

          std::string username = data["username"];
          std::string password = data["password"];

          if (db_manager_->userExists(username)) {
            LOG_WARN << "Registration failed: Username already exists -> "
                     << username;
            return makeJsonResponse(409,
                                    "{\"error\":\"Username already exists\"}");
          }

          if (db_manager_->createUser(username, password)) {
            LOG_INFO << "User registered successfully: " << username;
            return makeJsonResponse(200, "{\"status\":\"success\"}");
          } else {
            return makeJsonResponse(500,
                                    "{\"error\":\"Internal server error\"}");
          }
        } catch (const json::exception& e) {
          return makeJsonResponse(400, "{\"error\":\"Invalid JSON format\"}");
        }
      });

  // ==========================================
  // 2. 用户登录 (/login)
  // ==========================================
  http_server_->addHandler(
      "/login", "POST",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          json data = json::parse(request.body);
          if (!data.contains("username") || !data.contains("password")) {
            return makeJsonResponse(400, "{\"error\":\"Missing credentials\"}");
          }

          std::string username = data["username"];
          std::string password = data["password"];

          if (db_manager_->validateUser(username, password)) {
            LOG_INFO << "User logged in: " << username;
            // 更新在线状态和活跃时间
            if (!db_manager_->setUserOnlineStatus(username, true)) {
              LOG_WARN << "Failed to set online status for " << username;
            }

            json response = {{"status", "success"}, {"username", username}};
            return makeJsonResponse(200, response.dump());  // 将 json 拍平
          } else {
            LOG_WARN << "Invalid login attempt for: " << username;
            return makeJsonResponse(
                401, "{\"error\":\"Invalid username or password\"}");
          }
        } catch (const json::exception& e) {
          return makeJsonResponse(400, "{\"error\":\"Invalid JSON format\"}");
        }
      });

  // ==========================================
  // 3. 用户登出 (/logout)
  // ==========================================
  http_server_->addHandler(
      "/logout", "POST",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          json data = json::parse(request.body);
          if (!data.contains("username"))
            return makeJsonResponse(400, "{\"error\":\"Missing username\"}");

          std::string username = data["username"];
          if (db_manager_->setUserOnlineStatus(username, false)) {
            LOG_INFO << "User logged out: " << username;
            return makeJsonResponse(200, "{\"status\":\"success\"}");
          }
          return makeJsonResponse(500, "{\"error\":\"Logout failed\"}");
        } catch (const json::exception& e) {
          return makeJsonResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });

  // ==========================================
  // 4. 获取所有用户列表 (/users)
  // ==========================================
  http_server_->addHandler(
      "/users", "GET",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        auto users = db_manager_->getAllUsers();
        json response = json::array();

        for (const auto& user : users) {
          response.push_back(
              {{"username", user.username}, {"is_online", user.is_online}});
        }
        return makeJsonResponse(200, response.dump());
      });

  // ==========================================
  // 5. 创建房间 (/create_room)
  // ==========================================
  http_server_->addHandler(
      "/create_room", "POST",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          json data = json::parse(request.body);
          if (!data.contains("name") || !data.contains("creator")) {
            return makeJsonResponse(
                400, "{\"error\":\"Missing room name or creator\"}");
          }

          std::string room_name = data["name"];
          std::string creator = data["creator"];

          if (db_manager_->createRoom(room_name, creator)) {
            // 建房成功后，自动把群主拉进群里
            if (!db_manager_->addRoomMember(room_name, creator)) {
              LOG_WARN << "Failed to auto-add " << creator << " to room";
            }
            LOG_INFO << "Room created: " << room_name << " by " << creator;
            return makeJsonResponse(200, "{\"status\":\"success\"}");
          }
          return makeJsonResponse(409,
                                  "{\"error\":\"Room name already exists\"}");
        } catch (const json::exception& e) {
          return makeJsonResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });

  // ==========================================
  // 6. 加入房间 (/join_room)
  // ==========================================
  http_server_->addHandler(
      "/join_room", "POST",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          json data = json::parse(request.body);
          if (!data.contains("room") || !data.contains("username")) {
            return makeJsonResponse(400, "{\"error\":\"Missing data\"}");
          }

          std::string room_name = data["room"];
          std::string username = data["username"];

          if (db_manager_->addRoomMember(room_name, username)) {
            LOG_INFO << username << " joined room: " << room_name;
            return makeJsonResponse(200, "{\"status\":\"success\"}");
          }
          return makeJsonResponse(404, "{\"error\":\"Room not found\"}");
        } catch (const json::exception& e) {
          return makeJsonResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });

  // ==========================================
  // 7. 获取房间列表 (/rooms)
  // ==========================================
  http_server_->addHandler(
      "/rooms", "GET",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        auto rooms = db_manager_->getRooms();
        json response = json::array();

        for (const auto& room : rooms) {
          response.push_back(
              {{"name", room}, {"members", db_manager_->getRoomMembers(room)}});
        }
        return makeJsonResponse(200, response.dump());
      });

  // ==========================================
  // 8. 发送消息 (/send_message)
  // ==========================================
  http_server_->addHandler(
      "/send_message", "POST",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          json data = json::parse(request.body);
          if (!data.contains("room") || !data.contains("username") ||
              !data.contains("content")) {
            return makeJsonResponse(400, "{\"error\":\"Missing fields\"}");
          }

          std::string room_name = data["room"];
          std::string username = data["username"];
          std::string content = data["content"];
          int64_t timestamp = getCurrentTimeMs();

          if (db_manager_->saveMessage(room_name, username, content,
                                       timestamp)) {
            LOG_DEBUG << "Message saved: [" << room_name << "] " << username
                      << ": " << content;
            return makeJsonResponse(200, "{\"status\":\"success\"}");
          }
          return makeJsonResponse(500,
                                  "{\"error\":\"Failed to save message\"}");
        } catch (const json::exception& e) {
          return makeJsonResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });

  // ==========================================
  // 9. 拉取消息 (/messages)
  // ==========================================
  http_server_->addHandler(
      "/messages", "POST",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          json data = json::parse(request.body);
          if (!data.contains("room") || !data.contains("since")) {
            return makeJsonResponse(400, "{\"error\":\"Missing fields\"}");
          }

          std::string room_name = data["room"];
          int64_t since = data["since"];

          auto messages = db_manager_->getMessages(room_name, since);

          // 如果顺带传了 username，就顺便当作一次“心跳包”，刷新活跃时间
          if (data.contains("username")) {
            if (!db_manager_->updateUserLastActiveTime(data["username"])) {
              LOG_WARN << "Failed to update heartbeat for " << data["username"];
            }
          }

          return makeJsonResponse(200, json(messages).dump());
        } catch (const json::exception& e) {
          return makeJsonResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });
}

}  // namespace chat