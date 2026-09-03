// benchmark for kv-server, like redis-benchmark:
//   -h host -p port -c connections -t seconds -P pipeline_depth
//   --set-pct N --get-pct N --del-pct N
//   --keys N --value-size N
//

#include <hdr/hdr_histogram.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <latch>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Config {
    std::string host = "127.0.0.1";
    int port = 8888;
    int connections = 4;
    int duration_sec = 10;
    int pipeline_depth = 16;
    int set_pct = 34;
    int get_pct = 33;
    int del_pct = 33;
    int key_count = 1000;
    int value_size = 64;
};

struct ThreadStats {
    hdr_histogram* histogram = nullptr;
    uint64_t completed_ops = 0;
    uint64_t errors = 0;
};

void write_all(int fd, const std::string& data) {
    size_t total_written = 0;
    while (total_written < data.size()) {
        ssize_t n = ::write(fd, data.data() + total_written, data.size() - total_written);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error(std::string("write failed: ") + std::strerror(errno));
        }
        total_written += static_cast<size_t>(n);
    }
}

bool read_more(int fd, std::string& buffer) {
    char chunk[4096];
    ssize_t n = ::read(fd, chunk, sizeof(chunk));
    if (n == 0) {
        return false; // EOF
    }
    if (n < 0) {
        if (errno == EINTR) {
            return true;
        }
        throw std::runtime_error(std::string("read failed: ") + std::strerror(errno));
    }
    buffer.append(chunk, n);
    return true;
}

int connect_to_server(const Config& cfg) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error(std::string("socket() failed: ") + std::strerror(errno));
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(cfg.port));
    if (::inet_pton(AF_INET, cfg.host.c_str(), &addr.sin_addr) != 1) {
        ::close(fd);
        throw std::runtime_error("invalid host address: " + cfg.host);
    }

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        throw std::runtime_error(std::string("connect() failed: ") + std::strerror(errno));
    }
    return fd;
}

std::string random_value(int size, std::mt19937& rng) {
    static const char kAlphabet[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<int> dist(0, sizeof(kAlphabet) - 2);
    std::string result(size, '\0');
    for (auto& c : result) {
        c = kAlphabet[dist(rng)];
    }
    return result;
}

std::string build_command(const Config& cfg, std::mt19937& rng,
                          std::uniform_int_distribution<int>& key_dist,
                          std::uniform_int_distribution<int>& roll_dist) {
    int key = key_dist(rng);
    int roll = roll_dist(rng);

    if (roll < cfg.set_pct) {
        return "SET key" + std::to_string(key) + " " + random_value(cfg.value_size, rng) + "\n";
    }
    if (roll < cfg.set_pct + cfg.get_pct) {
        return "GET key" + std::to_string(key) + "\n";
    }
    return "DEL key" + std::to_string(key) + "\n";
}

void worker_thread(const Config& cfg, std::latch& start_latch, std::atomic<bool>& stop_flag,
                   ThreadStats& stats) {
    int fd = connect_to_server(cfg);

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> key_dist(0, cfg.key_count - 1);
    std::uniform_int_distribution<int> roll_dist(0, 99);

    hdr_init(/*lowest*/ 1, /*highest*/ 60'000'000, /*significant_figures*/ 3, &stats.histogram);

    std::string recv_buffer;
    std::vector<std::chrono::steady_clock::time_point> send_times;
    send_times.reserve(cfg.pipeline_depth);

    start_latch.count_down();
    start_latch.wait();

    while (!stop_flag.load(std::memory_order_relaxed)) {
        send_times.clear();

        for (int i = 0; i < cfg.pipeline_depth; ++i) {
            std::string cmd = build_command(cfg, rng, key_dist, roll_dist);
            send_times.push_back(std::chrono::steady_clock::now());
            write_all(fd, cmd);
        }

        int responses_read = 0;
        while (responses_read < cfg.pipeline_depth) {
            size_t newline_pos = recv_buffer.find('\n');
            if (newline_pos == std::string::npos) {
                if (!read_more(fd, recv_buffer)) {
                    stop_flag.store(true, std::memory_order_relaxed);
                    ::close(fd);
                    return;
                }
                continue;
            }

            auto recv_time = std::chrono::steady_clock::now();
            recv_buffer.erase(0, newline_pos + 1);

            auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                  recv_time - send_times[responses_read])
                                  .count();
            hdr_record_value(stats.histogram, latency_us);

            ++responses_read;
            ++stats.completed_ops;
        }
    }

    ::close(fd);
}

Config parse_args(int argc, char** argv) {
    Config cfg;
    auto next_int = [&](int& i) { return std::atoi(argv[++i]); };

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h") {
            cfg.host = argv[++i];
        } else if (arg == "-p") {
            cfg.port = next_int(i);
        } else if (arg == "-c") {
            cfg.connections = next_int(i);
        } else if (arg == "-t") {
            cfg.duration_sec = next_int(i);
        } else if (arg == "-P") {
            cfg.pipeline_depth = next_int(i);
        } else if (arg == "--set-pct") {
            cfg.set_pct = next_int(i);
        } else if (arg == "--get-pct") {
            cfg.get_pct = next_int(i);
        } else if (arg == "--del-pct") {
            cfg.del_pct = next_int(i);
        } else if (arg == "--keys") {
            cfg.key_count = next_int(i);
        } else if (arg == "--value-size") {
            cfg.value_size = next_int(i);
        } else {
            std::cerr << "unknown argument: " << arg << "\n";
            std::exit(1);
        }
    }

    if (cfg.set_pct + cfg.get_pct + cfg.del_pct != 100) {
        std::cerr << "set-pct + get-pct + del-pct must equal 100\n";
        std::exit(1);
    }
    return cfg;
}

} // namespace

int main(int argc, char** argv) {
    Config cfg = parse_args(argc, argv);

    std::cout << "connections=" << cfg.connections << " duration=" << cfg.duration_sec << "s"
              << " pipeline=" << cfg.pipeline_depth << " ratio(set/get/del)=" << cfg.set_pct << "/"
              << cfg.get_pct << "/" << cfg.del_pct << " keys=" << cfg.key_count
              << " value_size=" << cfg.value_size << "\n";

    std::latch start_latch{static_cast<ptrdiff_t>(cfg.connections)};
    std::atomic<bool> stop_flag{false};
    std::vector<ThreadStats> stats(cfg.connections);
    std::vector<std::thread> threads;

    for (int i = 0; i < cfg.connections; ++i) {
        threads.emplace_back(worker_thread, std::cref(cfg), std::ref(start_latch),
                             std::ref(stop_flag), std::ref(stats[i]));
    }

    auto run_start = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::seconds(cfg.duration_sec));
    stop_flag.store(true, std::memory_order_relaxed);

    for (auto& t : threads) {
        t.join();
    }
    auto run_end = std::chrono::steady_clock::now();
    double actual_duration_sec = std::chrono::duration<double>(run_end - run_start).count();

    hdr_histogram* merged = nullptr;
    hdr_init(1, 60'000'000, 3, &merged);
    uint64_t total_ops = 0;
    uint64_t total_errors = 0;
    for (auto& s : stats) {
        hdr_add(merged, s.histogram);
        total_ops += s.completed_ops;
        total_errors += s.errors;
        hdr_close(s.histogram);
    }

    double rps = static_cast<double>(total_ops) / actual_duration_sec;

    std::cout << "\n--- results ---\n";
    std::cout << "total ops:    " << total_ops << "\n";
    std::cout << "errors:       " << total_errors << "\n";
    std::cout << "duration:     " << actual_duration_sec << "s\n";
    std::cout << "throughput:   " << rps << " ops/sec\n";
    std::cout << "latency (us): p50=" << hdr_value_at_percentile(merged, 50.0)
              << " p95=" << hdr_value_at_percentile(merged, 95.0)
              << " p99=" << hdr_value_at_percentile(merged, 99.0)
              << " p99.9=" << hdr_value_at_percentile(merged, 99.9) << " max=" << hdr_max(merged)
              << "\n";

    hdr_close(merged);
    return 0;
}