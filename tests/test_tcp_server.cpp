#include <gtest/gtest.h>

#include "tcp_server.hpp"

#include <cstdint>
#include <system_error>

namespace {

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

} // namespace