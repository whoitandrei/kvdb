#pragma once
#include "socket.hpp"
#include <cstdint>
#include <sys/socket.h>

class TcpServer {
public:
    explicit TcpServer(std::uint16_t port, int backlog = SOMAXCONN);
    ~TcpServer() = default;

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    TcpServer(TcpServer&&) = delete;
    TcpServer& operator=(TcpServer&&) = delete;

    std::uint16_t port() const noexcept { return port_; }

    void run();

private:
    Socket socket_;
    std::uint16_t port_ = 0;
};