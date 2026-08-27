#include "tcp_server.hpp"
#include "utils.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;

TcpServer::TcpServer(std::uint16_t port, int backlog) : port_(port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    socket_.reset(fd);
    if (!socket_) {
        throw_errno("socket");
    }
    std::cerr << GREEN << "[LOG]" << RESET << "socket created succesfully" << std::endl;

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    int opt = 1;
    if (::setsockopt(socket_.get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        throw_errno("setsockopt");
    }

    if (::bind(socket_.get(), reinterpret_cast<sockaddr*>(&server_address),
               sizeof(server_address)) != SUCCESS_RETURN) {
        throw_errno("bind");
    }
    std::cerr << GREEN << "[LOG]" << RESET << "socket bind succesfully" << std::endl;

    if (::listen(socket_.get(), backlog) != SUCCESS_RETURN) {
        throw_errno("listen");
    }
    std::cerr << GREEN << "[LOG]" << RESET << "listen() set succesfully. Ready to accept on port " << port_
              << std::endl;

    sockaddr_in actual{};
    socklen_t len = sizeof(actual);
    if (::getsockname(socket_.get(), reinterpret_cast<sockaddr*>(&actual), &len) == ERR_RETURN) {
        throw_errno("getsockname");
    }
    port_ = ntohs(actual.sin_port);
}

void TcpServer::run(Handler handler) {
    while (true) {
        int connfd = ::accept(socket_.get(), nullptr, nullptr);
        if (connfd == ERR_RETURN) {
            switch (errno) {
                case EINTR:
                case ECONNABORTED: continue;
                case ENFILE:
                case EMFILE: std::this_thread::sleep_for(10ms); continue;
                case EBADF:
                case EINVAL: return;
                default: throw_errno("accept");
            }
        }

        Socket client_socket(connfd);
        try {
            handler(std::move(client_socket));
        } catch (const std::exception& e) {
            std::cerr << RED "[ERROR] " << RESET << "exception occurred in TcpServer::run(): " << e.what() << std::endl;
        } catch (...) {
            std::cerr << RED "[ERROR] " << RESET << "exception occurred in TcpServer::run()" << std::endl;
        }
    }
}
