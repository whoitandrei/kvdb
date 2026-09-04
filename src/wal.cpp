#include "wal.hpp"

#include "utils.hpp"
#include "logger.hpp"
 
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <mutex>
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

    std::string buffer;
    buffer.reserve(sizeof(record_len) + record_len);
    append_uint32(buffer, record_len);

    // we can use bitmask here, but left the space for the future updates
    buffer.push_back(static_cast<char>(op));

    append_uint32(buffer, key_len);
    buffer.append(key);
    append_uint32(buffer, value_len);
    buffer.append(value);

    std::lock_guard<std::mutex> lock(mutex_);
    write_all(file_.get(), buffer.data(), sizeof(record_len) + record_len);
}

void Wal::replay(Store& store) const {
    ::lseek(file_.get(), 0, SEEK_SET);

    while (true) {
        char len_buf[sizeof(uint32_t)];
        if (!read_exact(file_.get(), len_buf, sizeof(len_buf))) break;

        uint32_t record_len = read_uint32(len_buf);

        char buffer[record_len];
        if (!read_exact(file_.get(), buffer, record_len)) break;

        std::string record(buffer);
        record.resize(record_len);

        size_t pos = 0;
        auto op = static_cast<Op>(record[pos]);
        pos += sizeof(uint8_t);

        uint32_t key_len = read_uint32(record.data() + pos);
        pos += sizeof(key_len);
        std::string key = record.substr(pos, key_len);
        pos += key_len;

        uint32_t value_len = read_uint32(record.data() + pos);
        pos += sizeof(value_len);
        std::string value = record.substr(pos, value_len);

        switch (op) {
            case Op::kSet:
                store.set(key, value);
                break;
            case Op::kDel:
                store.del(key);
                break;
        }
    }
}
