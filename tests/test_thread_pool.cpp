#include <atomic>
#include <chrono>
#include <exception>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "scheduler/concurrency/ThreadPool.hpp"

namespace {

using scheduler::concurrency::ThreadPool;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void ExecutesSingleTask() {
    ThreadPool pool{1};
    std::promise<int> promise;
    auto future = promise.get_future();

    pool.submit([&promise] {
        promise.set_value(42);
    });

    expect(future.wait_for(std::chrono::seconds(1)) == std::future_status::ready,
           "single task should complete");
    expect(future.get() == 42, "single task should produce the expected value");
    pool.shutdown();
}

void ExecutesMultipleTasks() {
    ThreadPool pool{2};
    constexpr int taskCount = 8;
    std::atomic<int> completed{0};
    std::promise<void> promise;
    auto future = promise.get_future();

    for (int i = 0; i < taskCount; ++i) {
        pool.submit([&] {
            if (completed.fetch_add(1) + 1 == taskCount) {
                promise.set_value();
            }
        });
    }

    expect(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready,
           "all submitted tasks should complete");
    expect(completed.load() == taskCount, "all tasks should be executed exactly once");
    pool.shutdown();
}

void ExecutesTasksConcurrently() {
    ThreadPool pool{4};
    constexpr int taskCount = 4;
    std::atomic<int> running{0};
    std::atomic<int> maxRunning{0};
    std::atomic<int> completed{0};
    std::promise<void> promise;
    auto future = promise.get_future();

    for (int i = 0; i < taskCount; ++i) {
        pool.submit([&] {
            const int current = running.fetch_add(1) + 1;

            int observed = maxRunning.load();
            while (current > observed &&
                   !maxRunning.compare_exchange_weak(observed, current)) {
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            running.fetch_sub(1);

            if (completed.fetch_add(1) + 1 == taskCount) {
                promise.set_value();
            }
        });
    }

    expect(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready,
           "concurrent tasks should complete");
    expect(maxRunning.load() > 1, "more than one task should run at the same time");
    pool.shutdown();
}

void ShutdownIsGraceful() {
    ThreadPool pool{2};
    constexpr int taskCount = 6;
    std::atomic<int> completed{0};

    for (int i = 0; i < taskCount; ++i) {
        pool.submit([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            completed.fetch_add(1);
        });
    }

    pool.shutdown();
    expect(completed.load() == taskCount, "shutdown should allow queued tasks to finish");
}

void SubmitAfterShutdownThrows() {
    ThreadPool pool{1};
    pool.shutdown();

    bool threw = false;
    try {
        pool.submit([] {});
    } catch (const std::runtime_error&) {
        threw = true;
    }

    expect(threw, "submit after shutdown should throw runtime_error");
}

void runTest(const char* name, void (*test)()) {
    test();
    std::cout << "[PASS] " << name << '\n';
}

}  // namespace

int main() {
    try {
        runTest("ExecutesSingleTask", &ExecutesSingleTask);
        runTest("ExecutesMultipleTasks", &ExecutesMultipleTasks);
        runTest("ExecutesTasksConcurrently", &ExecutesTasksConcurrently);
        runTest("ShutdownIsGraceful", &ShutdownIsGraceful);
        runTest("SubmitAfterShutdownThrows", &SubmitAfterShutdownThrows);
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
