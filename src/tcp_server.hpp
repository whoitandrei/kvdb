#pragma once
#include "socket.hpp"
#include "thread_pool.hpp"
#include "utils.hpp"
#include <cstdint>
#include <functional>
#include <sys/socket.h>

class TcpServer {
public:
    using Handler = std::function<void(Socket)>;

    explicit TcpServer(std::uint16_t port, int backlog = SOMAXCONN);
    ~TcpServer() = default;

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    TcpServer(TcpServer&&) = delete;
    TcpServer& operator=(TcpServer&&) = delete;

    std::uint16_t port() const noexcept { return port_; }

    void run(ThreadPool& pool, Handler handler);
    void stop();

private:
    Socket socket_;
    std::uint16_t port_ = 0;
    Socket wake_read_;
    Socket wake_write_;
};