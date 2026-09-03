#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>

enum class LogLevel {
    kDebug,
    kInfo,
    kWarning,
    kError,
};

class Logger {
  public:
    static Logger& instance();

    void log(LogLevel level, std::string message, const char* file, int line);
    void set_level(LogLevel level);

    LogLevel min_level() const { return min_level_.load(std::memory_order_relaxed); }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

  private:
    struct LogRecord {
        LogLevel level;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        std::thread::id thread_id;
        const char* file;
        int line;
    };

    Logger();
    ~Logger();

    void writer_loop();
    static std::string format(const LogRecord& record);
    static const char* level_to_string(LogLevel level);

    std::atomic<LogLevel> min_level_{LogLevel::kInfo};

    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<LogRecord> queue_;
    bool stop_ = false;
    std::thread writer_thread_;
};

class LogStream {
  public:
    LogStream(LogLevel level, const char* file, int line)
        : level_(level), file_(file), line_(line),
          enabled_(level >= Logger::instance().min_level()) {}

    ~LogStream() {
        if (enabled_) {
            Logger::instance().log(level_, stream_.str(), file_, line_);
        }
    }

    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;

    template <typename T> LogStream& operator<<(const T& value) {
        if (enabled_) {
            stream_ << value;
        }
        return *this;
    }

  private:
    LogLevel level_;
    const char* file_;
    int line_;
    bool enabled_;
    std::ostringstream stream_;
};

#define LOG_DEBUG() ::LogStream(LogLevel::kDebug, __FILE__, __LINE__)
#define LOG_INFO() ::LogStream(LogLevel::kInfo, __FILE__, __LINE__)
#define LOG_WARNING() ::LogStream(LogLevel::kWarning, __FILE__, __LINE__)
#define LOG_ERROR() ::LogStream(LogLevel::kError, __FILE__, __LINE__)