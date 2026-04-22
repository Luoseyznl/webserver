#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace chat {

struct User {
    std::string username;
    std::string password;
    bool is_online;

    User() : is_online(false) {}
    User(std::string name, std::string pwd, bool online = false) 
        : username(std::move(name)), password(std::move(pwd)), is_online(online) {}

    nlohmann::json toJson() const;
    static User fromJson(const nlohmann::json& j);
};

} // namespace chat