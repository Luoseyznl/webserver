# 源码（用来生成静态库的目标文件 .o 并归档 .a）
set(CORE_SOURCES
    chat_application.cc
    http/http_server.cc
    http/http_request.cc
    http/http_response.cc
    chat/user.cc
    utils/thread_pool.cc
    utils/logger.cc
    utils/timer.cc
    db/database_manager.cc
)

add_library(chat_core STATIC ${CORE_SOURCES})

# 源码文件的头文件路径（可被链接的文件传播，比如测试文件）：./src 和 ./third_party
target_include_directories(chat_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party
)

# 链接系统三方库文件（主程序 web_server 不需要知道系统三方库的头文件、编译配置）
target_link_libraries(chat_core PRIVATE
    Threads::Threads # Threads::Threads 是导入目标，内部封装了头文件路径、目标文件路径、链接选项
    SQLite::SQLite3
)

add_executable(chat_server main.cc)

# 链接本项目生成的库文件，设置 make install 时的安装规则（将 chat_server 安装到 {CMAKE_INSTALL_PREFIX}/bin
target_link_libraries(chat_server PRIVATE chat_core)

install(TARGETS chat_server RUNTIME DESTINATION bin) # RUNTIME 可执行文件 LIBRARY 动态库 ARCHIVE 静态库
