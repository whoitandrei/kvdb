#pragma once

#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class Store {
public:
    struct ScanResult {
        std::vector<std::pair<std::string, std::string>> items;
        std::size_t next_cursor = 0;
    };

    void set(std::string key, std::string value);
    std::optional<std::string> get(const std::string& key) const;
    bool del(const std::string& key);

    std::size_t size() const;
    ScanResult scan(std::size_t cursor, std::size_t count) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> data_;
};