#include "socket.hpp"
#include "tcp_server.hpp"
#include "thread_pool.hpp"

#include <csignal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <unistd.h>

namespace {

TcpServer* g_server = nullptr;

[[maybe_unused]] void handle_signal(int sig) {
    if (g_server != nullptr) {
        g_server->stop();
    }
}

void handle_connection(Socket client) {
    char buf[4096];
    while (true) {
        ssize_t n = ::read(client.get(), buf, sizeof(buf));

        if (n == 0) {
            return;
        }
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return;
        }

        ::write(client.get(), buf, static_cast<std::size_t>(n));
    }
}

} // namespace

int main() {
    ::signal(SIGPIPE, SIG_IGN);

    ThreadPool pool(4);
    TcpServer server(8888);

    g_server = &server;
    ::signal(SIGINT, handle_signal);

    std::cerr << "listening on port " << server.port() << '\n';

    server.run(pool, handle_connection);

    std::cerr << "server stopped\n";
    return 0;
}