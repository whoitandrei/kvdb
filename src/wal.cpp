#include "wal.hpp"

#include "utils.hpp"
#include "logger.hpp"
 
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
 
#include <cstring>

// low-level helpers
namespace {

void write_all(int fd, const char* data, size_t size) {
    size_t written = 0;
    while (written < size) {
        ssize_t n = ::write(fd, data + written, size - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            throw_errno("write [wal]");
        }
        written += static_cast<size_t>(n);
    }
}

bool read_exact(int fd, char* data, size_t size) {
    size_t read_total = 0;
    while (read_total < size) {
        ssize_t n = ::read(fd, data + read_total, size - read_total);
        if (n < 0) {
            if (errno == EINTR) continue;
            throw_errno("read [wal]");
        }
        if (n == 0) return false;
        read_total += static_cast<size_t>(n);
    }
    return true;
}

void append_uint32(std::string& buffer, uint32_t value) {
    char bytes[sizeof(value)];
    std::memcpy(bytes, &value, sizeof(value));
    buffer.append(bytes, sizeof(bytes));
}

uint32_t read_uint32(const char* data) {
    uint32_t value;
    std::memcpy(&value, data, sizeof(uint32_t));
    return value;
}

} // namespace

Wal::Wal(const std::string& path) {
    int fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        throw_errno("open [wal]");
    }
    file_.reset(fd);
}

void Wal::append_set(const std::string& key, const std::string& value) {
    append_record(Op::kSet, key, value);
}

void Wal::append_del(const std::string& key) {
    append_record(Op::kDel, key, "");
}

void Wal::append_record(Op op, const std::string& key, const std::string& value) {
    auto key_len = static_cast<uint32_t>(key.size());
    auto value_len = static_cast<uint32_t>(value.size());
    auto record_len = sizeof(uint8_t) + sizeof(key_len) + key_len + sizeof(value_len) + value_len;

    
}

void Wal::replay(Store& store) const {

}
