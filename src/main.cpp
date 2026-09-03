#include "logger.hpp"
#include "socket.hpp"
#include "store.hpp"
#include "tcp_server.hpp"
#include "thread_pool.hpp"
#include "utils.hpp"
#include "handler.hpp"
#include "logger.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
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
    std::string logger_level = "info";
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

LogLevel parse_log_level(const std::string& level_str) {
    if (level_str == "debug") {
        return LogLevel::kDebug;
    } else if (level_str == "info") {
        return LogLevel::kInfo;
    } else if (level_str == "warning") {
        return LogLevel::kWarning;
    } else if (level_str == "error") {
        return LogLevel::kError;
    } else {
        std::cerr << "invalid logger level: " << level_str << "\n";
        std::exit(1);
    }
}

Config parse_args(int argc, char** argv) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-p") {
            cfg.port = parse_positive_int(argc, argv, i, "-p");
        } else if (arg == "-w") {
            cfg.workers = parse_positive_int(argc, argv, i, "-w");
        } else if (arg == "-l") {
            cfg.logger_level = argv[++i];
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
    Logger::instance().set_level(parse_log_level(cfg.logger_level));

    g_server = &server;
    ::signal(SIGINT, handle_signal);

    LOG_INFO() << "listening on port " << server.port()
              << " with " << cfg.workers << " workers\n";

    server.run(pool, [&store](Socket client) {
        handle_connection(std::move(client), store);
    });

    LOG_INFO() << "server stopped\n";
    return 0;
}