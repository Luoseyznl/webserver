#include "db/database_manager.h"

#include <chrono>
#include <sstream>

#include "utils/logger.h"

namespace chat {

static int64_t getCurrentTimeMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

DatabaseManager::DatabaseManager(const std::string& db_path)
    : db_path_(db_path), db_(nullptr) {
  std::lock_guard<std::mutex> lock(mutex_);
  int rc = sqlite3_open(db_path_.c_str(), &db_);
  if (rc) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db_);
    return;
  }
  LOG_INFO << "Opened SQLite database successfully: " << db_path_;

  initializeTables();
}

DatabaseManager::~DatabaseManager() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (db_) {
    sqlite3_close(db_);
  }
}

bool DatabaseManager::executeQueryUnlocked(const std::string& query) {
  char* err_msg = nullptr;
  int rc = sqlite3_exec(db_, query.c_str(), nullptr, nullptr, &err_msg);

  if (rc != SQLITE_OK) {
    LOG_ERROR << "SQL error: " << err_msg;
    sqlite3_free(err_msg);
    return false;
  }
  return true;
}

bool DatabaseManager::initializeTables() {
  const char* create_users_table =
      "CREATE TABLE IF NOT EXISTS users ("
      "username TEXT PRIMARY KEY,"  // 自然主键（用户名是唯一的）
      "password_hash TEXT NOT NULL,"
      "created_at INTEGER NOT NULL,"
      "is_online INTEGER DEFAULT 0,"           // 新用户默认离线
      "last_active_time INTEGER DEFAULT 0);";  // 新用户会被心跳检测到

  const char* create_rooms_table =
      "CREATE TABLE IF NOT EXISTS rooms ("
      "name TEXT PRIMARY KEY,"
      "creator TEXT NOT NULL,"
      "created_at INTEGER NOT NULL,"
      "FOREIGN KEY(creator) REFERENCES users(username));";  // 外键约束

  const char* create_room_members_table =
      "CREATE TABLE IF NOT EXISTS room_members ("
      "room_name TEXT NOT NULL,"
      "username TEXT NOT NULL,"
      "joined_at INTEGER NOT NULL,"
      "PRIMARY KEY(room_name, username),"  // 复合主键
      "FOREIGN KEY(room_name) REFERENCES rooms(name),"
      "FOREIGN KEY(username) REFERENCES users(username));";

  const char* create_messages_table =
      "CREATE TABLE IF NOT EXISTS messages ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"  // 自增 ID 主键
      "room_name TEXT NOT NULL,"
      "username TEXT NOT NULL,"
      "content TEXT NOT NULL,"
      "timestamp INTEGER NOT NULL,"
      "FOREIGN KEY(room_name) REFERENCES rooms(name),"
      "FOREIGN KEY(username) REFERENCES users(username));";

  return executeQueryUnlocked(create_users_table) &&
         executeQueryUnlocked(create_rooms_table) &&
         executeQueryUnlocked(create_room_members_table) &&
         executeQueryUnlocked(create_messages_table);
}

bool DatabaseManager::createUser(const std::string& username,
                                 const std::string& password_hash) {
  std::lock_guard<std::mutex> lock(mutex_);
  // 1. 定义 SQL 模板，通过 (?, ?, ?) 占位，预防 SQL 注入攻击
  const char* sql =
      "INSERT INTO users (username, password_hash, created_at) VALUES (?, ?, "
      "?);";

  sqlite3_stmt* stmt;  // 指向“经过数据库解析后的虚拟机字节码”结构体
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Failed to prepare statement: " << sqlite3_errmsg(db_);
    return false;
  }

  // 绑定 *stmt 对应的参数位
  sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 3, getCurrentTimeMs());

  bool success = (sqlite3_step(stmt) == SQLITE_DONE);  // 执行成功
  sqlite3_finalize(stmt);                              // 清理
  return success;
}

bool DatabaseManager::validateUser(const std::string& username,
                                   const std::string& password_hash) {
  std::lock_guard<std::mutex> lock(mutex_);
  const char* sql =
      "SELECT COUNT(*) FROM users WHERE username = ? AND password_hash = ?;";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_STATIC);

  bool valid = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {  // SELECT COUNT(*) 会返回一行
    valid = (sqlite3_column_int(stmt, 0) > 0);
  }

  sqlite3_finalize(stmt);
  return valid;
}

bool DatabaseManager::userExists(const std::string& username) {
  std::lock_guard<std::mutex> lock(mutex_);
  const char* sql = "SELECT COUNT(*) FROM users WHERE username = ?;";
  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

  bool exists = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    exists = (sqlite3_column_int(stmt, 0) > 0);
  }
  sqlite3_finalize(stmt);
  return exists;
}

bool DatabaseManager::saveMessage(const std::string& room_name,
                                  const std::string& username,
                                  const std::string& content,
                                  int64_t timestamp) {
  std::lock_guard<std::mutex> lock(mutex_);
  const char* sql =
      "INSERT INTO messages (room_name, username, content, timestamp) VALUES "
      "(?, ?, ?, ?);";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_text(stmt, 1, room_name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 4, timestamp);

  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

std::vector<nlohmann::json> DatabaseManager::getMessages(
    const std::string& room_name, int64_t since) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<nlohmann::json> messages;

  // 拉取最近消息时间戳以后的消息
  const char* sql =
      "SELECT username, content, timestamp FROM messages WHERE room_name = ? "
      "AND timestamp > ? ORDER BY timestamp ASC;";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return messages;

  sqlite3_bind_text(stmt, 1, room_name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 2, since);

  // 循环拉取，直到取完
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    nlohmann::json msg;
    msg["username"] =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    msg["content"] =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    msg["timestamp"] = sqlite3_column_int64(stmt, 2);
    messages.push_back(msg);
  }

  sqlite3_finalize(stmt);
  return messages;
}

bool DatabaseManager::createRoom(const std::string& name,
                                 const std::string& creator) {
  std::lock_guard<std::mutex> lock(mutex_);
  const char* sql =
      "INSERT INTO rooms (name, creator, created_at) VALUES (?, ?, ?);";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, creator.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 3, getCurrentTimeMs());

  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

bool DatabaseManager::deleteRoom(const std::string& name) {
  std::lock_guard<std::mutex> lock(mutex_);
  const char* sql = "DELETE FROM rooms WHERE name = ?;";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

std::vector<std::string> DatabaseManager::getRooms() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::string> rooms;
  const char* sql = "SELECT name FROM rooms;";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return rooms;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    rooms.push_back(
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
  }

  sqlite3_finalize(stmt);
  return rooms;
}

bool DatabaseManager::addRoomMember(const std::string& room_name,
                                    const std::string& username) {
  std::lock_guard<std::mutex> lock(mutex_);
  // 如果此人已在群里，系统自动忽略报错并返回成功
  const char* sql =
      "INSERT OR IGNORE INTO room_members (room_name, username, joined_at) "
      "VALUES (?, ?, ?);";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_text(stmt, 1, room_name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 3, getCurrentTimeMs());

  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

bool DatabaseManager::removeRoomMember(const std::string& room_name,
                                       const std::string& username) {
  std::lock_guard<std::mutex> lock(mutex_);
  const char* sql =
      "DELETE FROM room_members WHERE room_name = ? AND username = ?;";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_text(stmt, 1, room_name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);

  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

std::vector<std::string> DatabaseManager::getRoomMembers(
    const std::string& room_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::string> members;
  const char* sql = "SELECT username FROM room_members WHERE room_name = ?;";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return members;

  sqlite3_bind_text(stmt, 1, room_name.c_str(), -1, SQLITE_STATIC);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    members.push_back(
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
  }

  sqlite3_finalize(stmt);
  return members;
}

bool DatabaseManager::setUserOnlineStatus(const std::string& username,
                                          bool is_online) {
  std::lock_guard<std::mutex> lock(mutex_);

  const char* sql = nullptr;
  if (is_online) {
    sql =
        "UPDATE users SET is_online = 1, last_active_time = ? WHERE username = "
        "?;";
  } else {
    sql = "UPDATE users SET is_online = 0 WHERE username = ?;";
  }

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  if (is_online) {
    sqlite3_bind_int64(stmt, 1, getCurrentTimeMs());
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
  }

  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

bool DatabaseManager::updateUserLastActiveTime(const std::string& username) {
  std::lock_guard<std::mutex> lock(mutex_);
  const char* sql = "UPDATE users SET last_active_time = ? WHERE username = ?;";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_int64(stmt, 1, getCurrentTimeMs());
  sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);

  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

bool DatabaseManager::checkAndUpdateInactiveUsers(int64_t timeout_ms) {
  std::lock_guard<std::mutex> lock(mutex_);
  int64_t timeout_time = getCurrentTimeMs() - timeout_ms;

  const char* sql =
      "UPDATE users SET is_online = 0 WHERE is_online = 1 AND "
      "last_active_time < ?;";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_int64(stmt, 1, timeout_time);

  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

std::vector<User> DatabaseManager::getAllUsers() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<User> users;
  const char* sql = "SELECT username, password_hash, is_online FROM users;";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Failed to prepare statement: " << sqlite3_errmsg(db_);
    return users;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* username =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    const char* password =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    bool is_online = (sqlite3_column_int(stmt, 2) > 0);

    users.push_back(
        User(std::string(username), std::string(password), is_online));
  }

  sqlite3_finalize(stmt);
  return users;
}

}  // namespace chat