# ==========================================
# 阶段 1：构建阶段 (Builder)
# ==========================================
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# libsqlite3-dev 是开发库，包含了头文件，编译时必需
RUN apt-get update && \
    apt-get install -y build-essential cmake libsqlite3-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# 将根目录下的所有文件（除了 .dockerignore）拷进容器的 /app 里
COPY . .

RUN mkdir -p build && cd build && \
    cmake -DBUILD_TESTS=OFF .. && \
    make install -j$(nproc)

# ==========================================
# 阶段 2：运行阶段 (Runtime)
# ==========================================
# 重新拿一个的 Ubuntu 镜像，丢弃 builder 镜像
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# libsqlite3-0 是运行库
RUN apt-get update && \
    apt-get install -y libsqlite3-0 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app/bin

# 跨阶段拷贝
COPY --from=builder /app/build/bin/chat_server .
COPY --from=builder /app/build/bin/static ./static

# 声明服务器监听的是 8080 端口
EXPOSE 8080

CMD ["./chat_server"]