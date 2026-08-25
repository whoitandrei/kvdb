#include <gtest/gtest.h>

#include "socket.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(!std::is_copy_constructible_v<Socket>,
              "Socket must not be copyable: two owners = double close");
static_assert(!std::is_copy_assignable_v<Socket>, "Socket must not be copy-assignable");
static_assert(std::is_nothrow_move_constructible_v<Socket>, "move ctor must be noexcept");
static_assert(std::is_nothrow_move_assignable_v<Socket>, "move assignment must be noexcept");
static_assert(std::is_nothrow_default_constructible_v<Socket>, "default ctor must be noexcept");

namespace {

bool is_fd_open(int fd) {
    return ::fcntl(fd, F_GETFD) != -1;
}

class SocketTest : public ::testing::Test {
  protected:
    int make_fd() {
        int fds[2];
        EXPECT_EQ(::pipe(fds), 0) << "pipe() failed, errno=" << errno;
        cleanup_fds_.push_back(fds[1]);
        return fds[0];
    }

    void TearDown() override {
        for (int fd : cleanup_fds_) {
            ::close(fd);
        }
    }

  private:
    std::vector<int> cleanup_fds_;
};

// RAII tests

TEST_F(SocketTest, DefaultConstructedIsEmpty) {
    Socket s;
    EXPECT_FALSE(static_cast<bool>(s));
    EXPECT_EQ(s.get(), -1);
}

TEST_F(SocketTest, ConstructorTakesOwnership) {
    const int fd = make_fd();
    Socket s(fd);
    EXPECT_TRUE(static_cast<bool>(s));
    EXPECT_EQ(s.get(), fd);
}

TEST_F(SocketTest, ConstructedFromInvalidIsEmpty) {
    Socket s(-1);
    EXPECT_FALSE(static_cast<bool>(s));
}

TEST_F(SocketTest, DestructorClosesFd) {
    const int fd = make_fd();
    ASSERT_TRUE(is_fd_open(fd));
    {
        Socket s(fd);
    }
    EXPECT_FALSE(is_fd_open(fd));
}

TEST_F(SocketTest, DestructorOfEmptySocketIsSafe) {
    // Не должно быть close(-1) и уж тем более падения.
    {
        Socket s;
    }
    SUCCEED();
}

// MOVE

TEST_F(SocketTest, MoveConstructorTransfersOwnership) {
    const int fd = make_fd();
    Socket a(fd);
    Socket b(std::move(a));

    EXPECT_FALSE(static_cast<bool>(a)) << "источник обязан опустеть";
    EXPECT_EQ(b.get(), fd);
    EXPECT_TRUE(is_fd_open(fd)) << "перемещение не должно ничего закрывать";
}

TEST_F(SocketTest, MovedFromSocketDoesNotCloseOnDestruction) {
    const int fd = make_fd();
    Socket b;
    {
        Socket a(fd);
        b = std::move(a);
    }
    EXPECT_TRUE(is_fd_open(fd));
    EXPECT_EQ(b.get(), fd);
}

TEST_F(SocketTest, MoveAssignmentClosesOwnFd) {
    const int old_fd = make_fd();
    const int new_fd = make_fd();

    Socket target(old_fd);
    Socket source(new_fd);

    target = std::move(source);

    EXPECT_FALSE(is_fd_open(old_fd)) << "старый дескриптор утёк";
    EXPECT_EQ(target.get(), new_fd);
    EXPECT_FALSE(static_cast<bool>(source));
}

TEST_F(SocketTest, SelfMoveAssignmentLeavesSocketValid) {
    const int fd = make_fd();
    Socket a(fd);

    Socket* p = &a;
    a = std::move(*p);

    EXPECT_TRUE(static_cast<bool>(a)) << "объект не должен опустеть";
    EXPECT_EQ(a.get(), fd);
    EXPECT_TRUE(is_fd_open(fd)) << "дескриптор не должен закрыться";
}

TEST_F(SocketTest, WorksInsideVectorReallocation) {
    const int fd1 = make_fd();
    const int fd2 = make_fd();

    std::vector<Socket> v;
    v.reserve(1);
    v.emplace_back(fd1);
    v.emplace_back(fd2); // здесь вектор реаллоцируется

    EXPECT_TRUE(is_fd_open(fd1));
    EXPECT_TRUE(is_fd_open(fd2));
    EXPECT_EQ(v[0].get(), fd1);
    EXPECT_EQ(v[1].get(), fd2);
}

// access to descriptor

TEST_F(SocketTest, GetDoesNotTransferOwnership) {
    const int fd = make_fd();
    {
        Socket s(fd);
        EXPECT_EQ(s.get(), fd);
        EXPECT_TRUE(is_fd_open(fd));
    }
    EXPECT_FALSE(is_fd_open(fd));
}

TEST_F(SocketTest, ReleaseGivesUpOwnershipWithoutClosing) {
    const int fd = make_fd();
    int released = -1;
    {
        Socket s(fd);
        released = s.release();
        EXPECT_FALSE(static_cast<bool>(s)) << "после release объект пуст";
        EXPECT_EQ(s.get(), -1);
    }

    EXPECT_EQ(released, fd);
    EXPECT_TRUE(is_fd_open(fd)) << "release не закрывает — это делает вызывающий";

    ::close(released);
    EXPECT_FALSE(is_fd_open(released));
}

TEST_F(SocketTest, ReleaseOnEmptySocketReturnsInvalid) {
    Socket s;
    EXPECT_EQ(s.release(), -1);
    EXPECT_FALSE(static_cast<bool>(s));
}

// reset  and swap

TEST_F(SocketTest, ResetClosesOldFdAndTakesNew) {
    const int old_fd = make_fd();
    const int new_fd = make_fd();

    Socket s(old_fd);
    s.reset(new_fd);

    EXPECT_FALSE(is_fd_open(old_fd)) << "reset обязан закрыть старый дескриптор";
    EXPECT_EQ(s.get(), new_fd);
    EXPECT_TRUE(is_fd_open(new_fd));
}

TEST_F(SocketTest, ResetWithoutArgumentEmptiesSocket) {
    const int fd = make_fd();
    Socket s(fd);

    s.reset();

    EXPECT_FALSE(static_cast<bool>(s));
    EXPECT_FALSE(is_fd_open(fd));
}

TEST_F(SocketTest, ResetOnEmptySocketIsSafe) {
    Socket s;
    s.reset();
    EXPECT_FALSE(static_cast<bool>(s));
}

TEST_F(SocketTest, MemberSwapExchangesOwnership) {
    const int fd1 = make_fd();
    const int fd2 = make_fd();

    Socket a(fd1);
    Socket b(fd2);
    a.swap(b);

    EXPECT_EQ(a.get(), fd2);
    EXPECT_EQ(b.get(), fd1);
    EXPECT_TRUE(is_fd_open(fd1)) << "swap ничего не закрывает";
    EXPECT_TRUE(is_fd_open(fd2));
}

TEST_F(SocketTest, FreeSwapIsFoundViaAdl) {
    const int fd1 = make_fd();
    const int fd2 = make_fd();

    Socket a(fd1);
    Socket b(fd2);

    using std::swap;
    swap(a, b);

    EXPECT_EQ(a.get(), fd2);
    EXPECT_EQ(b.get(), fd1);
}

TEST_F(SocketTest, SwapWithEmptySocket) {
    const int fd = make_fd();
    Socket a(fd);
    Socket b;

    a.swap(b);

    EXPECT_FALSE(static_cast<bool>(a));
    EXPECT_EQ(b.get(), fd);
    EXPECT_TRUE(is_fd_open(fd));
}

} // namespace