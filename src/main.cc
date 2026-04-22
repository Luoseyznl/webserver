#include <exception>
#include <iostream>

#include "chat_application.h"
#include "utils/logger.h"

int main() {
  try {
    utils::Logger::init();
    LOG_INFO << "Chat Server is starting...";

    chat::ChatApplication app(8080, "chat.db");
    app.start();  // 阻塞...

  } catch (const std::exception& e) {
    LOG_ERROR << "Fatal Error: " << e.what();
    return 1;
  } catch (...) {
    LOG_ERROR << "Fatal Error: Unknown exception occurred!";
    return 1;
  }

  LOG_INFO << "Chat Server has shut down gracefully.";
  return 0;
}