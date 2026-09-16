#pragma once
#include <cstddef>
#include <string>

enum class CommandType {
    kSet,
    kGet,
    kDel,
    kPing,
    kDbSize,
    kScan,
    kInfo,
    kInvalid,
};

struct Command {
    CommandType type = CommandType::kInvalid;
    std::string key;
    std::string value;
    std::size_t cursor = 0;
    std::size_t count = 0;
    std::string error_message;
};

Command parse_command(const std::string& line);