#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

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

void TimedOutTaskRetriesAndSucceeds() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 1};

    std::atomic<int> attempts{0};
    Job job{10, "timeout-retry-success"};
    job.addTask(Task{1, "task-1", [&] {
        if (attempts.fetch_add(1) == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
            return;
        }
    }, 1, std::chrono::milliseconds{50}});

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(10);

    const auto persistedJob = repository->getJob(10);
    expect(persistedJob.has_value(), "job should be persisted");
    const auto* persistedTask = persistedJob->getTask(1);

    expect(scheduler.getJobStatus(10) == JobStatus::Succeeded,
           "job should succeed after timeout retry succeeds");
    expect(scheduler.getTaskStatus(10, 1) == TaskStatus::Succeeded,
           "timed out task should eventually succeed");
    expect(attempts.load() == 2, "task should run once, time out, and retry once");
    expect(persistedTask != nullptr && persistedTask->retryCount() == 1,
           "retry count should record the timeout retry");
    scheduler.shutdown();
}

void TimedOutTaskExhaustsRetriesAndFailsJob() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 1};

    std::atomic<int> attempts{0};
    Job job{11, "timeout-retry-fail"};
    job.addTask(Task{1, "task-1", [&] {
        attempts.fetch_add(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }, 1, std::chrono::milliseconds{20}});

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(11);

    const auto persistedJob = repository->getJob(11);
    expect(persistedJob.has_value(), "job should be persisted");
    const auto* persistedTask = persistedJob->getTask(1);

    expect(scheduler.getJobStatus(11) == JobStatus::Failed,
           "job should fail when timeout retries are exhausted");
    expect(scheduler.getTaskStatus(11, 1) == TaskStatus::Failed,
           "task should be marked failed after timeout retry exhaustion");
    expect(attempts.load() == 2, "task should run initial attempt plus one retry");
    expect(persistedTask != nullptr && persistedTask->retryCount() == 1,
           "retry count should equal max retries after timeout exhaustion");
    scheduler.shutdown();
}

void TimeoutDoesNotUnlockDependentTaskUntilSuccess() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 1};

    std::atomic<int> attempts{0};
    std::atomic<bool> prerequisiteSucceeded{false};
    std::atomic<bool> dependentRan{false};

    Job job{12, "timeout-dependent"};
    job.addTask(Task{1, "task-1", [&] {
        if (attempts.fetch_add(1) == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
            return;
        }

        prerequisiteSucceeded.store(true);
    }, 1, std::chrono::milliseconds{50}});
    job.addTask(Task{2, "task-2", [&] {
        expect(prerequisiteSucceeded.load(),
               "dependent task should not run before prerequisite eventually succeeds");
        dependentRan.store(true);
    }});
    job.addDependency(2, 1);

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(12);

    expect(scheduler.getJobStatus(12) == JobStatus::Succeeded,
           "job should succeed once timed out prerequisite eventually succeeds");
    expect(scheduler.getTaskStatus(12, 1) == TaskStatus::Succeeded,
           "prerequisite should succeed after timeout retry");
    expect(scheduler.getTaskStatus(12, 2) == TaskStatus::Succeeded,
           "dependent task should run after prerequisite success");
    expect(attempts.load() == 2, "prerequisite should time out once and retry once");
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
        runTest("TimedOutTaskRetriesAndSucceeds", &TimedOutTaskRetriesAndSucceeds);
        runTest("TimedOutTaskExhaustsRetriesAndFailsJob",
                &TimedOutTaskExhaustsRetriesAndFailsJob);
        runTest("TimeoutDoesNotUnlockDependentTaskUntilSuccess",
                &TimeoutDoesNotUnlockDependentTaskUntilSuccess);
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
