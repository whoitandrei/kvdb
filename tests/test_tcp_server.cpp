#include <gtest/gtest.h>

#include "socket.hpp"
#include "tcp_server.hpp"
#include "thread_pool.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

constexpr auto kTimeout = 2s;

class Latch {
  public:
    void count_down() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            fired_ = true;
        }
        cv_.notify_all();
    }

    bool wait_for(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return fired_; });
    }

  private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool fired_ = false;
};

int connect_to(std::uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        return -1;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        ::close(fd);
        return -1;
    }
    return fd;
}

int open_fd_count() {
    int count = 0;
    for (int fd = 0; fd < 256; ++fd) {
        if (::fcntl(fd, F_GETFD) != -1) {
            ++count;
        }
    }
    return count;
}

// ===========================================================================
// construction / binding
// ===========================================================================

TEST(TcpServerTest, BindsToEphemeralPort) {
    TcpServer server(0);
    EXPECT_NE(server.port(), 0) << "getsockname did not report the real port";
}

TEST(TcpServerTest, EachServerGetsItsOwnEphemeralPort) {
    TcpServer a(0);
    TcpServer b(0);
    EXPECT_NE(a.port(), b.port());
}

TEST(TcpServerTest, SecondBindOnBusyPortThrows) {
    TcpServer first(0);
    const std::uint16_t busy = first.port();

    EXPECT_THROW({ TcpServer second(busy); }, std::system_error);
}

TEST(TcpServerTest, PortIsFreedAfterDestruction) {
    std::uint16_t port = 0;
    {
        TcpServer server(0);
        port = server.port();
    }

    EXPECT_NO_THROW({
        TcpServer reopened(port);
        EXPECT_EQ(reopened.port(), port);
    });
}

// ===========================================================================
// stop() from another thread
// ===========================================================================

TEST(TcpServerStopTest, StopFromAnotherThreadReturnsFromRun) {
    TcpServer server(0);
    ThreadPool pool(2);

    Latch run_returned;
    std::thread runner([&] {
        server.run(pool, [](Socket) { /* echo not needed here */ });
        run_returned.count_down();
    });

    int client_fd = -1;
    for (int attempt = 0; attempt < 50 && client_fd == -1; ++attempt) {
        client_fd = connect_to(server.port());
        if (client_fd == -1) {
            std::this_thread::sleep_for(10ms);
        }
    }
    ASSERT_NE(client_fd, -1) << "could not connect to test server";
    ::close(client_fd);

    server.stop();

    ASSERT_TRUE(run_returned.wait_for(kTimeout)) << "run() did not return after stop()";
    runner.join();
    pool.shutdown();
}

TEST(TcpServerStopTest, StopBeforeRunPreventsBlocking) {
    TcpServer server(0);
    ThreadPool pool(2);

    server.stop();

    Latch run_returned;
    std::thread runner([&] {
        server.run(pool, [](Socket) {});
        run_returned.count_down();
    });

    ASSERT_TRUE(run_returned.wait_for(kTimeout)) << "run() blocked despite prior stop()";
    runner.join();
    pool.shutdown();
}

// ===========================================================================
// idempotence
// ===========================================================================

TEST(TcpServerStopTest, StopIsIdempotent) {
    TcpServer server(0);
    server.stop();
    server.stop();
    server.stop();
    SUCCEED();
}

TEST(TcpServerStopTest, ConcurrentStopCallsAreSafe) {
    TcpServer server(0);

    std::vector<std::thread> stoppers;
    for (int i = 0; i < 8; ++i) {
        stoppers.emplace_back([&server] { server.stop(); });
    }
    for (auto& t : stoppers) {
        t.join();
    }
    SUCCEED();
}

// ===========================================================================
// descriptor hygiene
// ===========================================================================

TEST(TcpServerStopTest, SelfPipeDoesNotLeakDescriptors) {
    const int before = open_fd_count();

    {
        TcpServer server(0);
        ThreadPool pool(2);

        Latch run_returned;
        std::thread runner([&] {
            server.run(pool, [](Socket) {});
            run_returned.count_down();
        });

        std::this_thread::sleep_for(20ms);
        server.stop();

        ASSERT_TRUE(run_returned.wait_for(kTimeout));
        runner.join();
        pool.shutdown();
    }

    const int after = open_fd_count();
    EXPECT_EQ(after, before) << "TcpServer leaked file descriptors";
}

} // namespace