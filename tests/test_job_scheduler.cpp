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

void IndependentTasksRun() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 2};

    std::atomic<int> completed{0};
    Job job{1, "independent"};
    job.addTask(Task{1, "task-1", [&] { completed.fetch_add(1); }});
    job.addTask(Task{2, "task-2", [&] { completed.fetch_add(1); }});

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(1);

    expect(scheduler.getJobStatus(1) == JobStatus::Succeeded,
           "job with independent tasks should succeed");
    expect(scheduler.getTaskStatus(1, 1) == TaskStatus::Succeeded,
           "first task should succeed");
    expect(scheduler.getTaskStatus(1, 2) == TaskStatus::Succeeded,
           "second task should succeed");
    expect(completed.load() == 2, "both tasks should run");
    scheduler.shutdown();
}

void DependentTasksRunInOrder() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 2};

    std::mutex sequenceMutex;
    std::vector<int> sequence;
    std::atomic<bool> firstDone{false};

    Job job{2, "dependent"};
    job.addTask(Task{1, "task-1", [&] {
        std::lock_guard<std::mutex> lock(sequenceMutex);
        sequence.push_back(1);
        firstDone.store(true);
    }});
    job.addTask(Task{2, "task-2", [&] {
        expect(firstDone.load(), "dependent task ran before prerequisite completed");
        std::lock_guard<std::mutex> lock(sequenceMutex);
        sequence.push_back(2);
    }});
    job.addDependency(2, 1);

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(2);

    expect(scheduler.getJobStatus(2) == JobStatus::Succeeded,
           "dependent job should succeed");
    expect(sequence.size() == 2 && sequence[0] == 1 && sequence[1] == 2,
           "dependent tasks should run in dependency order");
    scheduler.shutdown();
}

void DiamondDependencyRunsCorrectly() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 4};

    std::atomic<bool> rootDone{false};
    std::atomic<bool> leftDone{false};
    std::atomic<bool> rightDone{false};

    Job job{3, "diamond"};
    job.addTask(Task{1, "root", [&] { rootDone.store(true); }});
    job.addTask(Task{2, "left", [&] {
        expect(rootDone.load(), "left task started before root completed");
        leftDone.store(true);
    }});
    job.addTask(Task{3, "right", [&] {
        expect(rootDone.load(), "right task started before root completed");
        rightDone.store(true);
    }});
    job.addTask(Task{4, "final", [&] {
        expect(leftDone.load() && rightDone.load(),
               "final task started before both prerequisites completed");
    }});
    job.addDependency(2, 1);
    job.addDependency(3, 1);
    job.addDependency(4, 2);
    job.addDependency(4, 3);

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(3);

    expect(scheduler.getJobStatus(3) == JobStatus::Succeeded,
           "diamond dependency job should succeed");
    expect(scheduler.getTaskStatus(3, 4) == TaskStatus::Succeeded,
           "final task should succeed");
    scheduler.shutdown();
}

void FailedTaskFailsJob() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 1};

    Job job{4, "failing"};
    job.addTask(Task{1, "task-1", [] {
        throw std::runtime_error("boom");
    }});
    job.addTask(Task{2, "task-2", [] {}});
    job.addDependency(2, 1);

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(4);

    expect(scheduler.getJobStatus(4) == JobStatus::Failed,
           "job should fail when a task fails");
    expect(scheduler.getTaskStatus(4, 1) == TaskStatus::Failed,
           "failing task should be marked failed");
    scheduler.shutdown();
}

void CycleFailsJob() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 2};

    Job job{5, "cycle"};
    job.addTask(Task{1, "task-1", [] {}});
    job.addTask(Task{2, "task-2", [] {}});
    job.addDependency(1, 2);
    job.addDependency(2, 1);

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(5);

    expect(scheduler.getJobStatus(5) == JobStatus::Failed,
           "cyclic job should fail immediately");
    scheduler.shutdown();
}

void ShutdownUnblocksWaiters() {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, 1};

    Job job{6, "shutdown"};
    job.addTask(Task{1, "task-1", [] {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }});
    job.addTask(Task{2, "task-2", [] {}});
    job.addDependency(2, 1);

    scheduler.submitJob(std::move(job));

    auto waiter = std::async(std::launch::async, [&scheduler] {
        scheduler.waitForJob(6);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    scheduler.shutdown();

    expect(waiter.wait_for(std::chrono::seconds(1)) == std::future_status::ready,
           "waitForJob should unblock when shutdown resolves the job");
    expect(scheduler.getJobStatus(6) != JobStatus::Running,
           "job should reach a terminal status after shutdown");
}

void runTest(const char* name, void (*test)()) {
    test();
    std::cout << "[PASS] " << name << '\n';
}

}  // namespace

int main() {
    try {
        runTest("IndependentTasksRun", &IndependentTasksRun);
        runTest("DependentTasksRunInOrder", &DependentTasksRunInOrder);
        runTest("DiamondDependencyRunsCorrectly", &DiamondDependencyRunsCorrectly);
        runTest("FailedTaskFailsJob", &FailedTaskFailsJob);
        runTest("CycleFailsJob", &CycleFailsJob);
        runTest("ShutdownUnblocksWaiters", &ShutdownUnblocksWaiters);
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
