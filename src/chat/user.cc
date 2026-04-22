#include "user.h"

namespace chat {

nlohmann::json User::toJson() const {
  return {{"username", username}, {"is_online", is_online}};
}

User User::fromJson(const nlohmann::json& j) {
  User u;
  if (j.contains("username")) u.username = j.at("username").get<std::string>();
  if (j.contains("password")) u.password = j.at("password").get<std::string>();
  if (j.contains("is_online")) u.is_online = j.at("is_online").get<bool>();
  return u;
}

}  // namespace chat