#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <queue>
#include "task.hpp"

class ThreadPool {
  public:
    explicit ThreadPool(std::size_t worker_count = std::thread::hardware_concurrency());
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    bool submit(Task task);
    void shutdown();

    size_t worker_count() const { return workers_.size(); }
    size_t pending_tasks() {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        return tasks_.size();
    }

  private:
    void worker_thread();

    std::vector<std::thread> workers_;
    std::queue<Task> tasks_;
    std::mutex tasks_mutex_;
    std::condition_variable condition_;
    std::once_flag stop_flag_;
    bool stop_ = false;
};
