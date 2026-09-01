#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

class Store {
public:
    void set(std::string key, std::string value);
    std::optional<std::string> get(const std::string& key) const;
    bool del(const std::string& key);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> data_;
};