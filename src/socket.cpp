#include "socket.hpp"

#include <utility>
#include <unistd.h>

Socket::~Socket() noexcept {
    reset();
}

Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = kInvalid;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    reset(other.release());
    return *this;
}

Socket::operator bool() const noexcept {
    return fd_ != kInvalid;
}

int Socket::release() noexcept {
    int old_fd = fd_;
    fd_ = kInvalid;
    return old_fd;
}

void Socket::reset(int fd) noexcept {
    if (fd_ != kInvalid) {
        ::close(fd_);
    }
    fd_ = fd;
}

void Socket::swap(Socket& other) noexcept {
    std::swap(fd_, other.fd_);
}
