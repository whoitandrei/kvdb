#include "protocol.hpp"
#include <iostream>
#include <string>

Command parse_command(const std::string& line) {
    const size_t firstSpace = line.find(' ');

    if (firstSpace == std::string::npos) {
        return {CommandType::kInvalid, "", ""};
    }

    const std::string commandStr = line.substr(0, firstSpace);

    if (commandStr == "SET") {
        const size_t secondSpace = line.find(' ', firstSpace + 1);

        if (secondSpace == std::string::npos) {
            return {CommandType::kInvalid, "", ""};
        }
        if (secondSpace == firstSpace + 1) {
            return {CommandType::kInvalid, "", ""};
        }
        if (secondSpace == line.size() - 1) {
            return {CommandType::kInvalid, "", ""};
        }

        const std::string key = line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
        const std::string value = line.substr(secondSpace + 1);

        return {CommandType::kSet, key, value};
    } else if (commandStr == "GET") {
        if (firstSpace + 1 == line.size()) {
            return {CommandType::kInvalid, "", ""};
        }
        if (line.find(' ', firstSpace + 1) != std::string::npos) {
            return {CommandType::kInvalid, "", ""};
        }

        const std::string key = line.substr(firstSpace + 1);

        return {CommandType::kGet, key, ""};
    } else if (commandStr == "DEL") {

        if (firstSpace + 1 == line.size()) {
            return {CommandType::kInvalid, "", ""};
        }
        if (line.find(' ', firstSpace + 1) != std::string::npos) {
            return {CommandType::kInvalid, "", ""};
        }

        const std::string key = line.substr(firstSpace + 1);

        return {CommandType::kDel, key, ""};
    }

    return {CommandType::kInvalid, "", ""};
}