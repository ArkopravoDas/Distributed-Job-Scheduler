#include <chrono>
#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "scheduler/concurrency/BlockingQueue.hpp"

namespace {

using scheduler::concurrency::BlockingQueue;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void PushPopSingleItem() {
    BlockingQueue<std::unique_ptr<int>> queue;

    queue.push(std::make_unique<int>(42));

    std::unique_ptr<int> value;
    const bool popped = queue.pop(value);

    expect(popped, "pop should succeed for a queued item");
    expect(value != nullptr, "popped value should not be null");
    expect(*value == 42, "popped value should match the pushed item");
    expect(queue.empty(), "queue should be empty after popping the only item");
}

void PopBlocksUntilPush() {
    BlockingQueue<int> queue;
    std::promise<int> resultPromise;
    auto resultFuture = resultPromise.get_future();

    std::thread consumer([&] {
        int value = 0;
        const bool popped = queue.pop(value);
        if (popped) {
            resultPromise.set_value(value);
        }
    });

    const auto statusBeforePush = resultFuture.wait_for(std::chrono::milliseconds(100));
    expect(statusBeforePush == std::future_status::timeout, "pop should block before an item is pushed");

    queue.push(7);

    const auto statusAfterPush = resultFuture.wait_for(std::chrono::seconds(1));
    expect(statusAfterPush == std::future_status::ready, "pop should complete after push");
    expect(resultFuture.get() == 7, "consumer should receive the pushed value");

    consumer.join();
}

void ShutdownWakesWaitingThread() {
    BlockingQueue<int> queue;
    std::promise<bool> resultPromise;
    auto resultFuture = resultPromise.get_future();

    std::thread consumer([&] {
        int value = 0;
        resultPromise.set_value(queue.pop(value));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    queue.shutdown();

    const auto status = resultFuture.wait_for(std::chrono::seconds(1));
    expect(status == std::future_status::ready, "shutdown should wake a waiting consumer");
    expect(resultFuture.get() == false, "pop should return false after shutdown on an empty queue");

    consumer.join();
}

void MultipleProducersConsumers() {
    BlockingQueue<int> queue;
    constexpr int producerCount = 4;
    constexpr int consumerCount = 4;
    constexpr int itemsPerProducer = 50;
    constexpr int totalItems = producerCount * itemsPerProducer;

    std::mutex resultsMutex;
    std::vector<int> consumedItems;
    consumedItems.reserve(totalItems);

    std::vector<std::thread> producers;
    for (int producerIndex = 0; producerIndex < producerCount; ++producerIndex) {
        producers.emplace_back([&, producerIndex] {
            for (int itemIndex = 0; itemIndex < itemsPerProducer; ++itemIndex) {
                queue.push((producerIndex * itemsPerProducer) + itemIndex);
            }
        });
    }

    std::vector<std::thread> consumers;
    for (int consumerIndex = 0; consumerIndex < consumerCount; ++consumerIndex) {
        consumers.emplace_back([&] {
            while (true) {
                int value = 0;
                if (!queue.pop(value)) {
                    break;
                }

                std::lock_guard<std::mutex> lock(resultsMutex);
                consumedItems.push_back(value);
            }
        });
    }

    for (auto& producer : producers) {
        producer.join();
    }

    queue.shutdown();

    for (auto& consumer : consumers) {
        consumer.join();
    }

    expect(static_cast<int>(consumedItems.size()) == totalItems, "all produced items should be consumed");
    expect(queue.empty(), "queue should be empty after shutdown and full consumption");
}

void runTest(const char* name, void (*test)()) {
    test();
    std::cout << "[PASS] " << name << '\n';
}

}  // namespace

int main() {
    try {
        runTest("PushPopSingleItem", &PushPopSingleItem);
        runTest("PopBlocksUntilPush", &PopBlocksUntilPush);
        runTest("ShutdownWakesWaitingThread", &ShutdownWakesWaitingThread);
        runTest("MultipleProducersConsumers", &MultipleProducersConsumers);
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
