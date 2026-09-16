#include "executor.hpp"

#include "stats.hpp"
#include "store.hpp"
#include "thread_pool.hpp"
#include "wal.hpp"

#include <string>

namespace {

std::string info_body(ServerContext& ctx) {
    const Stats& s = ctx.stats;

    std::string body;
    std::size_t fields = 0;

    auto add = [&](const char* name, std::uint64_t value) {
        body += name;
        body += ':';
        body += std::to_string(value);
        body += '\n';
        ++fields;
    };

    add("uptime_seconds", s.uptime_seconds());
    add("connections_current",
        static_cast<std::uint64_t>(s.connections_current.load(std::memory_order_relaxed)));
    add("connections_total", s.connections_total.load(std::memory_order_relaxed));
    add("keys", ctx.store.size());
    add("pool_workers", ctx.pool.worker_count());
    add("pool_queue_depth", ctx.pool.pending_tasks());
    add("cmd_set", s.cmd_set.load(std::memory_order_relaxed));
    add("cmd_get", s.cmd_get.load(std::memory_order_relaxed));
    add("cmd_del", s.cmd_del.load(std::memory_order_relaxed));
    add("cmd_scan", s.cmd_scan.load(std::memory_order_relaxed));
    add("cmd_ping", s.cmd_ping.load(std::memory_order_relaxed));
    add("cmd_dbsize", s.cmd_dbsize.load(std::memory_order_relaxed));
    add("cmd_info", s.cmd_info.load(std::memory_order_relaxed));
    add("cmd_invalid", s.cmd_invalid.load(std::memory_order_relaxed));

    return "FIELDS " + std::to_string(fields) + "\n" + body;
}

} // namespace

std::string execute(const Command& cmd, ServerContext& ctx) {
    switch (cmd.type) {
    case CommandType::kSet: {
        ctx.stats.cmd_set.fetch_add(1, std::memory_order_relaxed);
        ctx.wal.append_set(cmd.key, cmd.value);
        ctx.store.set(cmd.key, cmd.value);
        return "OK\n";
    }
    case CommandType::kGet: {
        ctx.stats.cmd_get.fetch_add(1, std::memory_order_relaxed);
        auto value = ctx.store.get(cmd.key);
        return value.has_value() ? "VALUE " + *value + "\n" : "NOT_FOUND\n";
    }
    case CommandType::kDel: {
        ctx.stats.cmd_del.fetch_add(1, std::memory_order_relaxed);
        ctx.wal.append_del(cmd.key);
        return ctx.store.del(cmd.key) ? "OK\n" : "NOT_FOUND\n";
    }
    case CommandType::kPing: {
        ctx.stats.cmd_ping.fetch_add(1, std::memory_order_relaxed);
        return "PONG\n";
    }
    case CommandType::kDbSize: {
        ctx.stats.cmd_dbsize.fetch_add(1, std::memory_order_relaxed);
        return "SIZE " + std::to_string(ctx.store.size()) + "\n";
    }
    case CommandType::kScan: {
        ctx.stats.cmd_scan.fetch_add(1, std::memory_order_relaxed);
        const std::size_t count = cmd.count > kMaxScanCount ? kMaxScanCount : cmd.count;
        const Store::ScanResult page = ctx.store.scan(cmd.cursor, count);

        std::string response = "ITEMS " + std::to_string(page.items.size()) + " " +
                               std::to_string(page.next_cursor) + "\n";
        for (const auto& [key, value] : page.items) {
            response += "KV " + key + " " + value + "\n";
        }
        return response;
    }
    case CommandType::kInfo: {
        ctx.stats.cmd_info.fetch_add(1, std::memory_order_relaxed);
        return info_body(ctx);
    }
    case CommandType::kInvalid:
        ctx.stats.cmd_invalid.fetch_add(1, std::memory_order_relaxed);
        return "ERROR: " + cmd.error_message + "\n";
    }
    return "ERROR: unhandled command\n";
}