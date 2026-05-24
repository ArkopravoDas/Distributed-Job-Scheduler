#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "scheduler/distributed/WorkerRegistry.hpp"

namespace {

using scheduler::distributed::WorkerRegistry;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void RegisterWorkerCreatesAliveEntry() {
    WorkerRegistry registry;
    registry.registerWorker("worker-1");

    expect(registry.exists("worker-1"), "registered worker should exist");
    expect(registry.isWorkerAlive("worker-1"), "registered worker should start alive");
}

void HeartbeatUpdatesKnownWorker() {
    WorkerRegistry registry;
    registry.registerWorker("worker-1");

    expect(registry.recordHeartbeat("worker-1"),
           "heartbeat should succeed for a registered worker");
    expect(registry.isWorkerAlive("worker-1"),
           "worker should remain alive after heartbeat");
}

void MissingHeartbeatMarksWorkerDead() {
    WorkerRegistry registry;
    registry.registerWorker("worker-1");

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const auto deadWorkers = registry.markDeadWorkers(std::chrono::milliseconds(10));

    expect(deadWorkers.size() == 1 && deadWorkers.front() == "worker-1",
           "worker should be marked dead after missed heartbeat");
    expect(!registry.isWorkerAlive("worker-1"),
           "worker should be marked not alive after missed heartbeat");
}

void HeartbeatRevivesWorker() {
    WorkerRegistry registry;
    registry.registerWorker("worker-1");

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    static_cast<void>(registry.markDeadWorkers(std::chrono::milliseconds(10)));

    expect(registry.recordHeartbeat("worker-1"),
           "heartbeat should still succeed for known worker");
    expect(registry.isWorkerAlive("worker-1"),
           "heartbeat should mark a known worker alive again");
}

void runTest(const char* name, void (*test)()) {
    test();
    std::cout << "[PASS] " << name << '\n';
}

}  // namespace

int main() {
    try {
        runTest("RegisterWorkerCreatesAliveEntry", &RegisterWorkerCreatesAliveEntry);
        runTest("HeartbeatUpdatesKnownWorker", &HeartbeatUpdatesKnownWorker);
        runTest("MissingHeartbeatMarksWorkerDead", &MissingHeartbeatMarksWorkerDead);
        runTest("HeartbeatRevivesWorker", &HeartbeatRevivesWorker);
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
