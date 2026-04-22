#pragma once

#include <sqlite3.h>

#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "chat/user.h"

namespace chat {

class DatabaseManager {
 public:
  DatabaseManager(const DatabaseManager&) = delete;
  DatabaseManager& operator=(const DatabaseManager&) = delete;

  explicit DatabaseManager(const std::string& db_path);
  ~DatabaseManager();

  [[nodiscard]] bool createUser(const std::string& username,
                                const std::string& password_hash);
  [[nodiscard]] bool validateUser(const std::string& username,
                                  const std::string& password_hash);
  [[nodiscard]] bool userExists(const std::string& username);
  [[nodiscard]] bool setUserOnlineStatus(const std::string& username,
                                         bool is_online);
  [[nodiscard]] bool updateUserLastActiveTime(const std::string& username);
  [[nodiscard]] bool checkAndUpdateInactiveUsers(int64_t timeout_ms);

  [[nodiscard]] std::vector<User> getAllUsers();

  [[nodiscard]] bool createRoom(const std::string& name,
                                const std::string& creator);
  [[nodiscard]] bool deleteRoom(const std::string& name);
  [[nodiscard]] std::vector<std::string> getRooms();

  [[nodiscard]] bool addRoomMember(const std::string& room_name,
                                   const std::string& username);
  [[nodiscard]] bool removeRoomMember(const std::string& room_name,
                                      const std::string& username);
  [[nodiscard]] std::vector<std::string> getRoomMembers(
      const std::string& room_name);

  [[nodiscard]] bool saveMessage(const std::string& room_name,
                                 const std::string& username,
                                 const std::string& content, int64_t timestamp);

  [[nodiscard]] std::vector<nlohmann::json> getMessages(
      const std::string& room_name, int64_t since = 0);

 private:
  // --- 私有无锁辅助函数 (Unlocked Helpers) ---
  bool initializeTables();
  bool executeQueryUnlocked(const std::string& query);

  sqlite3* db_;
  std::string db_path_;
  std::mutex mutex_;
};

}  // namespace chat