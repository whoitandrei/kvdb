#include "logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::Logger() {
    writer_thread_ = std::thread(&Logger::writer_loop, this);
}

Logger::~Logger() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }
}

void Logger::set_level(LogLevel level) {
    min_level_.store(level, std::memory_order_relaxed);
}

void Logger::log(LogLevel level, std::string message, const char* file, int line) {
    if (level < min_level_.load(std::memory_order_relaxed)) {
        return;
    }

    LogRecord record{
        level,
        std::move(message),
        std::chrono::system_clock::now(),
        std::this_thread::get_id(),
        file,
        line,
    };

    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(record));
    }
    cv_.notify_one();
}

void Logger::writer_loop() {
    while (true) {
        std::queue<LogRecord> local_batch;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });

            if (queue_.empty() && stop_) {
                return;
            }

            std::swap(local_batch, queue_);
        }

        while (!local_batch.empty()) {
            std::cerr << format(local_batch.front()) << '\n';
            local_batch.pop();
        }
    }
}

const char* Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::kDebug:
            return "DEBUG";
        case LogLevel::kInfo:
            return "INFO";
        case LogLevel::kWarning:
            return "WARNING";
        case LogLevel::kError:
            return "ERROR";
    }
    return "UNKNOWN";
}

std::string Logger::format(const LogRecord& record) {
    auto time = std::chrono::system_clock::to_time_t(record.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  record.timestamp.time_since_epoch()) %
              1000;

    std::ostringstream out;
    std::tm tm_buf = *std::localtime(&time);

    out << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count()
        << " [" << level_to_string(record.level) << "]"
        << " [thread " << record.thread_id << "]"
        << " " << record.file << ":" << record.line
        << " - " << record.message;

    return out.str();
}