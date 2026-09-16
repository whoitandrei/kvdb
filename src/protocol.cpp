#include "protocol.hpp"

#include <charconv>
#include <string_view>

namespace {

constexpr std::string_view kSpaces = " \t\r\n";

Command invalid(std::string message) {
    Command cmd;
    cmd.type = CommandType::kInvalid;
    cmd.error_message = std::move(message);
    return cmd;
}

std::string_view trim(std::string_view sv) {
    const std::size_t begin = sv.find_first_not_of(kSpaces);
    if (begin == std::string_view::npos) {
        return {};
    }
    const std::size_t end = sv.find_last_not_of(kSpaces);
    return sv.substr(begin, end - begin + 1);
}

std::string_view take_token(std::string_view& sv) {
    const std::size_t end = sv.find_first_of(kSpaces);
    std::string_view token = sv.substr(0, end);

    if (end == std::string_view::npos) {
        sv = {};
    } else {
        const std::size_t next = sv.find_first_not_of(kSpaces, end);
        sv = (next == std::string_view::npos) ? std::string_view{} : sv.substr(next);
    }
    return token;
}

std::string to_upper(std::string_view sv) {
    std::string out(sv);
    for (char& c : out) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 'a' && uc <= 'z') {
            c = static_cast<char>(uc - ('a' - 'A'));
        }
    }
    return out;
}

bool parse_size(std::string_view sv, std::size_t& out) {
    if (sv.empty()) {
        return false;
    }
    std::size_t value = 0;
    const char* first = sv.data();
    const char* last = sv.data() + sv.size();
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last) {
        return false;
    }
    out = value;
    return true;
}

Command parse_no_args(std::string_view rest, CommandType type, const char* name) {
    if (!rest.empty()) {
        return invalid(std::string(name) + " takes no arguments");
    }
    Command cmd;
    cmd.type = type;
    return cmd;
}

Command parse_one_key(std::string_view rest, CommandType type, const char* name) {
    if (rest.empty()) {
        return invalid(std::string("Empty key in ") + name + " command");
    }
    std::string_view key = take_token(rest);
    if (!rest.empty()) {
        return invalid(std::string("Invalid ") + name + " command format");
    }
    Command cmd;
    cmd.type = type;
    cmd.key = std::string(key);
    return cmd;
}

} // namespace

Command parse_command(const std::string& line) {
    std::string_view rest = trim(line);
    if (rest.empty()) {
        return invalid("Empty command line");
    }

    const std::string name = to_upper(take_token(rest));

    if (name == "SET") {
        if (rest.empty()) {
            return invalid("Empty key in SET command");
        }
        std::string_view key = take_token(rest);
        if (rest.empty()) {
            return invalid("Empty value in SET command");
        }
        Command cmd;
        cmd.type = CommandType::kSet;
        cmd.key = std::string(key);
        cmd.value = std::string(rest);
        return cmd;
    }
    if (name == "GET") {
        return parse_one_key(rest, CommandType::kGet, "GET");
    }
    if (name == "DEL") {
        return parse_one_key(rest, CommandType::kDel, "DEL");
    }
    if (name == "PING") {
        return parse_no_args(rest, CommandType::kPing, "PING");
    }
    if (name == "DBSIZE") {
        return parse_no_args(rest, CommandType::kDbSize, "DBSIZE");
    }
    if (name == "INFO") {
        return parse_no_args(rest, CommandType::kInfo, "INFO");
    }
    if (name == "SCAN") {
        std::string_view cursor_token = take_token(rest);
        std::string_view count_token = take_token(rest);
        if (cursor_token.empty() || count_token.empty()) {
            return invalid("SCAN requires cursor and count");
        }
        if (!rest.empty()) {
            return invalid("Invalid SCAN command format");
        }
        Command cmd;
        if (!parse_size(cursor_token, cmd.cursor)) {
            return invalid("SCAN: invalid cursor");
        }
        if (!parse_size(count_token, cmd.count) || cmd.count == 0) {
            return invalid("SCAN: invalid count");
        }
        cmd.type = CommandType::kScan;
        return cmd;
    }

    return invalid("Invalid command");
}