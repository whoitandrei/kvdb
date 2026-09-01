#include "store.hpp"

void Store::set(std::string key, std::string value) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.insert_or_assign(std::move(key), std::move(value));
}

std::optional<std::string> Store::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(key);

    if (it == data_.end()) {
        return std::nullopt;
    }

    return it->second;
}

bool Store::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<bool>(data_.erase(key));
}