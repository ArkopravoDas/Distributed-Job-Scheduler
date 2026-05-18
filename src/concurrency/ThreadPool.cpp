#include "scheduler/concurrency/ThreadPool.hpp"

#include <stdexcept>
#include <utility>

namespace scheduler::concurrency {

ThreadPool::ThreadPool(std::size_t workerCount) {
    workers_.reserve(workerCount);

    for (std::size_t i = 0; i < workerCount; ++i) {
        workers_.emplace_back(&ThreadPool::workerLoop, this);
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::submit(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (shutdown_) {
        throw std::runtime_error("thread pool is shut down");
    }

    tasks_.push(std::move(task));
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_) {
            return;
        }

        shutdown_ = true;
        tasks_.shutdown();
    }

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

std::size_t ThreadPool::workerCount() const {
    return workers_.size();
}

void ThreadPool::workerLoop() {
    std::function<void()> task;

    while (tasks_.pop(task)) {
        try {
            task();
        } catch (...) {
        }
    }
}

}  // namespace scheduler::concurrency
