#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "scheduler/concurrency/BlockingQueue.hpp"

namespace scheduler::concurrency {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t workerCount);
    ~ThreadPool();

    void submit(std::function<void()> task);
    void shutdown();
    std::size_t workerCount() const;

private:
    void workerLoop();

    BlockingQueue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
    mutable std::mutex mutex_;
    bool shutdown_{false};
};

}  // namespace scheduler::concurrency
