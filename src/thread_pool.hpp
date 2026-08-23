#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>

class ThreadPool {
    public:
    explicit ThreadPool(std::size_t worker_count = std::thread::hardware_concurrency());
    ~ThreadPool();

    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&)                 = delete;
    ThreadPool& operator=(ThreadPool&&)      = delete;

    bool submit(std::function<void()> task);
    void shutdown();

    size_t worker_count() const { return workers_.size(); }
    size_t pending_tasks() {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        return tasks_.size();
    }

private:
    void worker_thread();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex tasks_mutex_;
    std::condition_variable condition_;
    std::once_flag stop_flag_;
    bool stop_ = false;

};
