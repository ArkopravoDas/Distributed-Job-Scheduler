#include <exception>
#include <iostream>
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

void TaskThrowsExceptionJobFails() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 1};

    Job job{101, "throws"};
    job.addTask(Task{1, "task-1", [] { throw std::runtime_error("boom"); }});

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(101);

    expect(scheduler.getJobStatus(101) == JobStatus::Failed,
           "job should fail when task throws");
    expect(scheduler.getTaskStatus(101, 1) == TaskStatus::Failed,
           "throwing task should be marked failed");
    scheduler.shutdown();
}

void DependencyCycleJobFails() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 2};

    Job job{102, "cycle"};
    job.addTask(Task{1, "task-1", [] {}});
    job.addTask(Task{2, "task-2", [] {}});
    job.addDependency(1, 2);
    job.addDependency(2, 1);

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(102);

    expect(scheduler.getJobStatus(102) == JobStatus::Failed,
           "job should fail on dependency cycle");
    scheduler.shutdown();
}

void InvalidDependencyJobFails() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 2};

    Job job{103, "invalid-dependency"};
    job.addTask(Task{1, "task-1", [] {}});
    job.addDependency(1, 999);

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(103);

    expect(scheduler.getJobStatus(103) == JobStatus::Failed,
           "job should fail when dependency references unknown task");
    scheduler.shutdown();
}

void DuplicateTaskIdRejected() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 2};

    Job job{104, "duplicate-task"};
    job.addTask(Task{1, "task-1", [] {}});
    job.addTask(Task{1, "task-1-duplicate", [] {}});

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(104);

    expect(scheduler.getJobStatus(104) == JobStatus::Failed,
           "job should fail when duplicate task ids are present");
    scheduler.shutdown();
}

void SubmitAfterShutdownThrows() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 1};
    scheduler.shutdown();

    Job job{105, "after-shutdown"};
    job.addTask(Task{1, "task-1", [] {}});

    bool threw = false;
    try {
        scheduler.submitJob(std::move(job));
    } catch (const std::runtime_error&) {
        threw = true;
    }

    expect(threw, "submit after scheduler shutdown should throw");
}

void WaitForUnknownJobThrows() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 1};

    bool threw = false;
    try {
        scheduler.waitForJob(9999);
    } catch (const std::runtime_error&) {
        threw = true;
    }

    expect(threw, "waitForJob should throw for unknown job");
    scheduler.shutdown();
}

void GetUnknownTaskStatusThrows() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 1};

    Job job{106, "unknown-task"};
    job.addTask(Task{1, "task-1", [] {}});

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(106);

    bool threw = false;
    try {
        static_cast<void>(scheduler.getTaskStatus(106, 999));
    } catch (const std::runtime_error&) {
        threw = true;
    }

    expect(threw, "getTaskStatus should throw for unknown task");
    scheduler.shutdown();
}

void runTest(const char* name, void (*test)()) {
    test();
    std::cout << "[PASS] " << name << '\n';
}

}  // namespace

int main() {
    try {
        runTest("TaskThrowsExceptionJobFails", &TaskThrowsExceptionJobFails);
        runTest("DependencyCycleJobFails", &DependencyCycleJobFails);
        runTest("InvalidDependencyJobFails", &InvalidDependencyJobFails);
        runTest("DuplicateTaskIdRejected", &DuplicateTaskIdRejected);
        runTest("SubmitAfterShutdownThrows", &SubmitAfterShutdownThrows);
        runTest("WaitForUnknownJobThrows", &WaitForUnknownJobThrows);
        runTest("GetUnknownTaskStatusThrows", &GetUnknownTaskStatusThrows);
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
