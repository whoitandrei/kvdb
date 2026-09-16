#pragma once

#include "protocol.hpp"
#include <string>

class Store;
class Wal;
class ThreadPool;
struct Stats;

struct ServerContext {
    Store& store;
    Wal& wal;
    Stats& stats;
    ThreadPool& pool;
};

constexpr std::size_t kMaxScanCount = 1000;

std::string execute(const Command& cmd, ServerContext& ctx);