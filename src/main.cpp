#include "socket.hpp"
#include "store.hpp"
#include "tcp_server.hpp"
#include "thread_pool.hpp"
#include "utils.hpp"
#include "handler.hpp"

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

} // namespace

int main() {
    ::signal(SIGPIPE, SIG_IGN);

    Store store;
    ThreadPool pool(4);
    TcpServer server(8888);

    g_server = &server;
    ::signal(SIGINT, handle_signal);

    std::cerr << "listening on port " << server.port() << '\n';

    server.run(pool, [&store](Socket client) {
        handle_connection(std::move(client), store);
    });

    std::cerr << "server stopped\n";
    return 0;
}