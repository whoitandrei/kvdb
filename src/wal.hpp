#pragma once

#include "socket.hpp"
#include "store.hpp"

#include <cstdint>
#include <mutex>
#include <string>

// Формат записи на диске:
//
//   [uint32 record_len]              // длина всего, что после этого поля
//   [uint8  op]                      // 1 = SET, 2 = DEL
//   [uint32 key_len][key bytes]
//   [uint32 value_len][value bytes]  // value_len = 0 для DEL
//
// record_len ПЕРЕД остальными полями — чтобы при replay можно было
// сначала проверить, хватает ли в файле байт на всю запись, и только
// потом её парсить. Если не хватает (краш ровно посреди write()
// последней записи, вероятный сценарий без fsync) — это трактуется
// как штатно оборванный хвост, replay останавливается на нём, не
// пытаясь интерпретировать частично записанные байты как поля.

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