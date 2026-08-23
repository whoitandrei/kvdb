#include "thread_pool.hpp"
#include <iostream>
#include <mutex>

ThreadPool::ThreadPool(std::size_t worker_count) {
    try {
        if (worker_count == 0) {
            worker_count = 1;
            std::cerr << "[LOG] worker count is 0, defaulting to 1" << std::endl;
        }
        workers_.reserve(worker_count);
        for (std::size_t i = 0; i < worker_count; ++i) {
            workers_.emplace_back(&ThreadPool::worker_thread, this);
        }
    } catch (...) {
        std::cerr << "[LOG] exception occurred while initializing thread pool" << std::endl;
        shutdown(); 
        throw;
    }

}

ThreadPool::~ThreadPool() {
    try {
        shutdown();
    } catch (...) {
        std::cerr << "[LOG] exception occurred while shutting down thread pool" << std::endl;
    }
}

bool ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        if (stop_) {
            return false;
        }
        tasks_.push(std::move(task));
    }
    condition_.notify_one();
    return true;
}

void ThreadPool::shutdown() {
    std::call_once(stop_flag_, [this] {
        {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    });
}

void ThreadPool::worker_thread() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(tasks_mutex_);
            condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty()) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
        }

        try {
            task();
        } catch(std::exception& e) {
            std::cerr << "[LOG] exception occurred in task execution: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[LOG] exception occurred in task execution" << std::endl;
        }
    }
}