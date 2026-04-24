# C++ Web Chat Server

![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![CMake](https://img.shields.io/badge/CMake-3.14%2B-brightgreen.svg)
![Docker](https://img.shields.io/badge/docker-%230db7ed.svg?style=flat&logo=docker&logoColor=white)
[![C++ CI Pipeline](https://github.com/Luoseyznl/webserver/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/Luoseyznl/webserver/actions)

A real-time Web Chat System built with Modern C++ (C++17). 
This project serves as an exploration of building robust backend services, focusing on system architecture, protocol parsing, and safe concurrency.

## Features
- Custom-built HTTP Engine
- Incremental Message Sync
- Thread-safe Logger
- Concurrent Thread Pool
- Modern C++ features

## Dependencies
- C++17 compiler (GCC 8+, Clang 7+)
- CMake 3.14+
- SQLite3
- nlohmann/json (Included in `third_party`)

## Architecture & Design
The ChatApplication class acts as a **Facade** — a unified interface that hides the complexity of underlying subsystems (such as the HTTP server, routing, and database management).

```
.
├── static/                 # Frontend assets (HTML, CSS, Modern JS)
├── src/                    # Backend Source Code
│   ├── chat/
│   ├── db/
│   ├── http/          
│   ├── utils/        
|   ├── chat_application.cc
│   └── main.cc             # Entry point
├── tests/                  # GTest Unit Tests
└── third_party/            # Header-only libraries (nlohmann/json)
```

## Quick Start
1. Build & install
```sh
mkdir build && cd build
cmake ..
make install -j$(nproc)
```

2. Run the server
```sh
cd bin
./chat_server
```

3. Testing
```sh
cd build
ctest   # ./tests/unit_tests
```

4. Docker
```sh
docker build -t cpp-chat:v1 --network host --build-arg http_proxy=http://127.0.0.1:7890
docker run -d -p 8080:8080 --name my_chat_server cpp-chat:v1  
```
