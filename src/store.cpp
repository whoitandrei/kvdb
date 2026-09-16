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
std::size_t Store::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.size();
}

Store::ScanResult Store::scan(std::size_t cursor, std::size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);

    ScanResult result;
    const std::size_t buckets = data_.bucket_count();

    std::size_t bucket = cursor;
    while (bucket < buckets && result.items.size() < count) {
        for (auto it = data_.begin(bucket); it != data_.end(bucket); ++it) {
            result.items.emplace_back(it->first, it->second);
        }
        ++bucket;
    }

    result.next_cursor = (bucket >= buckets) ? 0 : bucket;
    return result;
}