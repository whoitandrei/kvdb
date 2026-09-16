#include "handler.hpp"
#include "stats.hpp"
#include "utils.hpp"
#include <unistd.h>

void handle_connection(Socket socket, ServerContext ctx) {
    ConnectionGuard connection(ctx.stats);

    std::string persistent_buffer;
    while (true) {
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
            std::string line = persistent_buffer.substr(0, newline_pos);
            persistent_buffer.erase(0, newline_pos + 1);

            const Command cmd = parse_command(line);
            const std::string response = execute(cmd, ctx);

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