#pragma once

#include "socket.hpp"
#include "store.hpp"

#include <cstdint>
#include <mutex>
#include <string>

//   [uint32 record_len]              // длина всего, что после этого поля
//   [uint8  op]                      // 1 = SET, 2 = DEL
//   [uint32 key_len][key bytes]
//   [uint32 value_len][value bytes]  // value_len = 0 для DEL

using FileDescriptor = Socket;

class Wal {
  public:
    explicit Wal(const std::string& path);

    void append_set(const std::string& key, const std::string& value);
    void append_del(const std::string& key);
    void replay(Store& store) const;

    Wal(const Wal&) = delete;
    Wal& operator=(const Wal&) = delete;

  private:
    enum class Op : uint8_t {
        kSet = 1,
        kDel = 2,
    };

    void append_record(Op op, const std::string& key, const std::string& value);

    std::mutex mutex_;
    FileDescriptor file_;
};