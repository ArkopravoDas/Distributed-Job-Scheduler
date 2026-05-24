#include <atomic>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "scheduler/core/Job.hpp"
#include "scheduler/core/JobStatus.hpp"
#include "scheduler/core/Task.hpp"
#include "scheduler/core/TaskStatus.hpp"
#include "scheduler/scheduling/JobScheduler.hpp"
#include "scheduler/storage/InMemoryJobRepository.hpp"

namespace {

using scheduler::core::Job;
using scheduler::core::JobStatus;
using scheduler::core::Task;
using scheduler::core::TaskStatus;
using scheduler::scheduling::JobScheduler;
using scheduler::storage::InMemoryJobRepository;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void FailedTaskRetriesAndSucceeds() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 1};

    std::atomic<int> attempts{0};
    Job job{7, "retry-success"};
    job.addTask(Task{1, "task-1", [&] {
        if (attempts.fetch_add(1) == 0) {
            throw std::runtime_error("first attempt fails");
        }
    }, 2});

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(7);

    const auto persistedJob = repository->getJob(7);
    expect(persistedJob.has_value(), "job should be persisted");
    const auto* persistedTask = persistedJob->getTask(1);

    expect(scheduler.getJobStatus(7) == JobStatus::Succeeded,
           "job should succeed after retry succeeds");
    expect(scheduler.getTaskStatus(7, 1) == TaskStatus::Succeeded,
           "retried task should be marked succeeded");
    expect(attempts.load() == 2, "task should run initial attempt plus one retry");
    expect(persistedTask != nullptr && persistedTask->retryCount() == 1,
           "retry count should record one retry");
    scheduler.shutdown();
}

void FailedTaskExhaustsRetriesAndFailsJob() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 1};

    std::atomic<int> attempts{0};
    Job job{8, "retry-exhausted"};
    job.addTask(Task{1, "task-1", [&] {
        attempts.fetch_add(1);
        throw std::runtime_error("always fails");
    }, 2});

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(8);

    const auto persistedJob = repository->getJob(8);
    expect(persistedJob.has_value(), "job should be persisted");
    const auto* persistedTask = persistedJob->getTask(1);

    expect(scheduler.getJobStatus(8) == JobStatus::Failed,
           "job should fail after retries are exhausted");
    expect(scheduler.getTaskStatus(8, 1) == TaskStatus::Failed,
           "task should be marked failed after retry exhaustion");
    expect(attempts.load() == 3, "task should run initial attempt plus max retries");
    expect(persistedTask != nullptr && persistedTask->retryCount() == 2,
           "retry count should equal max retries after exhaustion");
    scheduler.shutdown();
}

void RetryDoesNotUnlockDependentTaskUntilSuccess() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 1};

    std::atomic<int> attempts{0};
    std::atomic<bool> prerequisiteSucceeded{false};
    std::atomic<bool> dependentRan{false};

    Job job{9, "retry-dependent"};
    job.addTask(Task{1, "task-1", [&] {
        if (attempts.fetch_add(1) == 0) {
            throw std::runtime_error("first attempt fails");
        }

        prerequisiteSucceeded.store(true);
    }, 1});
    job.addTask(Task{2, "task-2", [&] {
        expect(prerequisiteSucceeded.load(),
               "dependent task should not run before prerequisite succeeds");
        dependentRan.store(true);
    }});
    job.addDependency(2, 1);

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(9);

    expect(scheduler.getJobStatus(9) == JobStatus::Succeeded,
           "job should succeed after prerequisite retry succeeds");
    expect(scheduler.getTaskStatus(9, 1) == TaskStatus::Succeeded,
           "prerequisite should succeed after retry");
    expect(scheduler.getTaskStatus(9, 2) == TaskStatus::Succeeded,
           "dependent should run after prerequisite succeeds");
    expect(attempts.load() == 2, "prerequisite should retry exactly once");
    expect(dependentRan.load(), "dependent task should eventually run");
    scheduler.shutdown();
}

void runTest(const char* name, void (*test)()) {
    test();
    std::cout << "[PASS] " << name << '\n';
}

}  // namespace

int main() {
    try {
        runTest("FailedTaskRetriesAndSucceeds", &FailedTaskRetriesAndSucceeds);
        runTest("FailedTaskExhaustsRetriesAndFailsJob",
                &FailedTaskExhaustsRetriesAndFailsJob);
        runTest("RetryDoesNotUnlockDependentTaskUntilSuccess",
                &RetryDoesNotUnlockDependentTaskUntilSuccess);
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
