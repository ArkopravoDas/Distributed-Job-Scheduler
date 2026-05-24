#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "scheduler/core/Job.hpp"
#include "scheduler/core/JobStatus.hpp"
#include "scheduler/core/Task.hpp"
#include "scheduler/core/TaskStatus.hpp"
#include "scheduler/distributed/CoordinatorServer.hpp"
#include "scheduler/distributed/WorkerClient.hpp"

namespace {

using scheduler::core::Job;
using scheduler::core::JobStatus;
using scheduler::core::Task;
using scheduler::core::TaskStatus;
using scheduler::distributed::CoordinatorServer;
using scheduler::distributed::WorkerClient;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Predicate>
bool waitUntil(Predicate predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    return predicate();
}

void RemoteTasksRunAcrossTwoWorkers() {
    CoordinatorServer coordinator;
    std::atomic<int> prerequisiteCount{0};
    std::atomic<int> finalCount{0};

    WorkerClient workerOne{"worker-1", [&](const std::string& message) {
                               return coordinator.handleRawMessage(message);
                           }};
    WorkerClient workerTwo{"worker-2", [&](const std::string& message) {
                               return coordinator.handleRawMessage(message);
                           }};

    auto prerequisiteHandler = [&] {
        prerequisiteCount.fetch_add(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(75));
    };

    auto finalHandler = [&] {
        if (prerequisiteCount.load() < 2) {
            throw std::runtime_error("prerequisites_not_done");
        }

        finalCount.fetch_add(1);
    };

    workerOne.registerTaskHandler("prepare", prerequisiteHandler);
    workerOne.registerTaskHandler("finish", finalHandler);
    workerTwo.registerTaskHandler("prepare", prerequisiteHandler);
    workerTwo.registerTaskHandler("finish", finalHandler);

    workerOne.registerWithCoordinator();
    workerTwo.registerWithCoordinator();
    workerOne.startHeartbeatLoop();
    workerTwo.startHeartbeatLoop();
    workerOne.startWorkLoop();
    workerTwo.startWorkLoop();

    Job job{100, "distributed-job"};
    job.addTask(Task{1, "prepare", [] {}});
    job.addTask(Task{2, "prepare", [] {}});
    job.addTask(Task{3, "finish", [] {}});
    job.addDependency(3, 1);
    job.addDependency(3, 2);

    coordinator.submitJob(std::move(job));
    coordinator.waitForJob(100);

    workerOne.stopWorkLoop();
    workerTwo.stopWorkLoop();
    workerOne.stopHeartbeatLoop();
    workerTwo.stopHeartbeatLoop();

    expect(coordinator.getJobStatus(100) == JobStatus::Succeeded,
           "distributed job should succeed");
    expect(coordinator.getTaskStatus(100, 1) == TaskStatus::Succeeded,
           "first prerequisite task should succeed");
    expect(coordinator.getTaskStatus(100, 2) == TaskStatus::Succeeded,
           "second prerequisite task should succeed");
    expect(coordinator.getTaskStatus(100, 3) == TaskStatus::Succeeded,
           "final task should succeed");
    expect(prerequisiteCount.load() == 2, "two prerequisite tasks should run");
    expect(finalCount.load() == 1, "final task should run once");
}

void DeadWorkerRunningTaskGetsRescheduled() {
    CoordinatorServer coordinator;
    std::atomic<int> completedBySecondWorker{0};

    WorkerClient workerOne{"worker-1", [&](const std::string& message) {
                               return coordinator.handleRawMessage(message);
                           }};
    WorkerClient workerTwo{"worker-2", [&](const std::string& message) {
                               return coordinator.handleRawMessage(message);
                           }};

    workerOne.registerTaskHandler("reschedulable", [&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    });
    workerTwo.registerTaskHandler("reschedulable", [&] {
        completedBySecondWorker.fetch_add(1);
    });

    workerOne.registerWithCoordinator();
    workerTwo.registerWithCoordinator();
    workerOne.startHeartbeatLoop();
    workerTwo.startHeartbeatLoop();
    workerOne.startWorkLoop();
    workerTwo.startWorkLoop();

    Job job{200, "reschedule-job"};
    job.addTask(Task{1, "reschedulable", [] {}});
    coordinator.submitJob(std::move(job));

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    workerOne.stopHeartbeatLoop();

    expect(waitUntil(
               [&] {
                   return !coordinator.markDeadWorkers(std::chrono::milliseconds(300)).empty();
               },
               std::chrono::milliseconds(1500)),
           "worker one should be marked dead");

    coordinator.waitForJob(200);

    workerOne.stopWorkLoop();
    workerTwo.stopWorkLoop();
    workerTwo.stopHeartbeatLoop();

    expect(coordinator.getJobStatus(200) == JobStatus::Succeeded,
           "job should succeed after reassignment");
    expect(coordinator.getTaskStatus(200, 1) == TaskStatus::Succeeded,
           "task should succeed after reassignment");
    expect(completedBySecondWorker.load() == 1,
           "second worker should complete the reassigned task");
}

void runTest(const char* name, void (*test)()) {
    test();
    std::cout << "[PASS] " << name << '\n';
}

}  // namespace

int main() {
    try {
        runTest("RemoteTasksRunAcrossTwoWorkers", &RemoteTasksRunAcrossTwoWorkers);
        runTest("DeadWorkerRunningTaskGetsRescheduled", &DeadWorkerRunningTaskGetsRescheduled);
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
