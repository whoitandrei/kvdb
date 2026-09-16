#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

struct Stats {
    using Clock = std::chrono::steady_clock;

    std::atomic<std::uint64_t> cmd_set{0};
    std::atomic<std::uint64_t> cmd_get{0};
    std::atomic<std::uint64_t> cmd_del{0};
    std::atomic<std::uint64_t> cmd_scan{0};
    std::atomic<std::uint64_t> cmd_ping{0};
    std::atomic<std::uint64_t> cmd_dbsize{0};
    std::atomic<std::uint64_t> cmd_info{0};
    std::atomic<std::uint64_t> cmd_invalid{0};

    std::atomic<std::uint64_t> connections_total{0};
    std::atomic<std::int64_t> connections_current{0};

    Clock::time_point started_at = Clock::now();

    std::uint64_t uptime_seconds() const {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - started_at).count());
    }
};

class ConnectionGuard {
public:
    explicit ConnectionGuard(Stats& stats) : stats_(stats) {
        stats_.connections_total.fetch_add(1, std::memory_order_relaxed);
        stats_.connections_current.fetch_add(1, std::memory_order_relaxed);
    }
    ~ConnectionGuard() {
        stats_.connections_current.fetch_sub(1, std::memory_order_relaxed);
    }
    ConnectionGuard(const ConnectionGuard&) = delete;
    ConnectionGuard& operator=(const ConnectionGuard&) = delete;

private:
    Stats& stats_;
};