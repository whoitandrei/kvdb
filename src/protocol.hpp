#pragma once
#include <string>

enum class CommandType {
    kSet,
    kGet,
    kDel,
    kInvalid,
};

struct Command {
    CommandType type;
    std::string key;
    std::string value;
    std::string error_message;
};

Command parse_command(const std::string& line);