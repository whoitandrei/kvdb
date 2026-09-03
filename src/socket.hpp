#pragma once

class Socket {
  public:
    explicit Socket(int fd) noexcept : fd_(fd) {}
    Socket() noexcept = default;
    ~Socket() noexcept;

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    int get() const noexcept { return fd_; }
    explicit operator bool() const noexcept;
    int release() noexcept;

    void reset(int fd = kInvalid) noexcept;
    void swap(Socket& other) noexcept;
    bool valid() const noexcept;

    friend void swap(Socket& a, Socket& b) noexcept { a.swap(b); }

  private:
    static constexpr int kInvalid = -1;
    int fd_ = kInvalid;
};