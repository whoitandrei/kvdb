#include "handler.hpp"
#include "utils.hpp"
#include <unistd.h>

void handle_connection(Socket socket, Store& store, Wal& wal) {
    std::string persistent_buffer;
    while (true) {
        // socket read
        constexpr size_t kBufferSize = 1024;
        char buffer[kBufferSize];
        ssize_t bytes_read = ::read(socket.get(), buffer, kBufferSize - 1);

        if (bytes_read == 0) {
            break;
        }
        if (bytes_read < 0) {
            switch (errno) {
            case EINTR:
                continue;
            case ECONNRESET:
                return;
            default:
                throw_errno("read");
            }
        }

        persistent_buffer.append(buffer, bytes_read);

        size_t newline_pos;
        while ((newline_pos = persistent_buffer.find('\n')) != std::string::npos) {

            // parse command
            std::string line = persistent_buffer.substr(0, newline_pos);
            persistent_buffer.erase(0, newline_pos + 1);

            Command cmd = parse_command(line);

            // store
            std::string response;
            switch (cmd.type) {
            case CommandType::kSet:
                wal.append_set(cmd.key, cmd.value);
                store.set(cmd.key, cmd.value);
                response = "OK\n";
                break;
            case CommandType::kGet: {
                auto value = store.get(cmd.key);
                if (value.has_value()) {
                    response = "VALUE " + value.value() + "\n";
                } else {
                    response = "NOT_FOUND\n";
                }
                break;
            }
            case CommandType::kDel: {
                wal.append_del(cmd.key);
                bool deleted = store.del(cmd.key);
                response = deleted ? "OK\n" : "NOT_FOUND\n";
                break;
            }
            case CommandType::kInvalid:
                response = "ERROR: " + cmd.error_message + "\n";
                break;
            }

            // socket write
            size_t total_written = 0;
            while (total_written < response.size()) {
                ssize_t n = ::write(socket.get(), response.data() + total_written,
                                    response.size() - total_written);

                if (n < 0) {
                    switch (errno) {
                    case EINTR:
                        continue;
                    case ECONNRESET:
                    case EPIPE:
                        return;
                    default:
                        throw_errno("write");
                    }
                }
                total_written += n;
            }
        }
    }
}