#include "socket.hpp"
#include "store.hpp"
#include "tcp_server.hpp"
#include "thread_pool.hpp"
#include "utils.hpp"
#include "handler.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unistd.h>

namespace {

TcpServer* g_server = nullptr;

void handle_signal(int sig) {
    if (g_server != nullptr) {
        g_server->stop();
    }
}

struct Config {
    int port = 8888;
    int workers = 4;
};

int parse_positive_int(int argc, char** argv, int& i, const char* flag_name) {
    if (i + 1 >= argc) {
        std::cerr << flag_name << " requires a value\n";
        std::exit(1);
    }
    ++i;
    char* end = nullptr;
    long value = std::strtol(argv[i], &end, 10);
    if (end == argv[i] || *end != '\0' || value <= 0) {
        std::cerr << flag_name << ": invalid value '" << argv[i] << "'\n";
        std::exit(1);
    }
    return static_cast<int>(value);
}

Config parse_args(int argc, char** argv) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-p") {
            cfg.port = parse_positive_int(argc, argv, i, "-p");
        } else if (arg == "-w") {
            cfg.workers = parse_positive_int(argc, argv, i, "-w");
        } else {
            std::cerr << "unknown argument: " << arg << "\n";
            std::exit(1);
        }
    }
    return cfg;
}

}  // namespace

int main(int argc, char** argv) {
    Config cfg = parse_args(argc, argv);

    ::signal(SIGPIPE, SIG_IGN);

    Store store;
    ThreadPool pool(cfg.workers);
    TcpServer server(cfg.port);

    g_server = &server;
    ::signal(SIGINT, handle_signal);

    std::cerr << "listening on port " << server.port()
              << " with " << cfg.workers << " workers\n";

    server.run(pool, [&store](Socket client) {
        handle_connection(std::move(client), store);
    });

    std::cerr << "server stopped\n";
    return 0;
}