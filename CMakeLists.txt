cmake_minimum_required(VERSION 3.14) # PROJECT_NAME = web_server
project(web_server LANGUAGES CXX) # CMAKE_CXX_COMPILER = /usr/bin/c++

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0") # 0 级优化

# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 下载外部的三方库源码（在 ./build/_deps/ 中）
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    include(FetchContent)
    fetchcontent_declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/heads/main.zip # gtest 和 gmock
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    fetchcontent_makeavailable(googletest)
    add_subdirectory(tests)
endif()

# 查找依赖的三方库目标文件和头文件
find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

# 源码目录：CMAKE_SOURCE_DIR（顶层 CMakeLists.txt 所在位置）
# 构建目录：CMAKE_BINARY_DIR（cmake .. 执行时所在位置）
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}") # make install 安装位置（默认 /usr/local）

enable_testing()

add_subdirectory(src)

# 将当前 CMakeLists.txt 所在目录下的 static 资源拷贝进构建目录下的 bin 里
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/static DESTINATION ${CMAKE_BINARY_DIR}/bin)
