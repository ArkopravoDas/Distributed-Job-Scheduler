#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "scheduler/core/Job.hpp"
#include "scheduler/core/JobStatus.hpp"
#include "scheduler/core/Task.hpp"
#include "scheduler/scheduling/JobScheduler.hpp"
#include "scheduler/storage/InMemoryJobRepository.hpp"

namespace {

using scheduler::core::Job;
using scheduler::core::JobId;
using scheduler::core::JobStatus;
using scheduler::core::Task;
using scheduler::core::TaskId;
using scheduler::scheduling::JobScheduler;
using scheduler::storage::InMemoryJobRepository;

constexpr auto kTaskWorkDuration = std::chrono::milliseconds{2};

const char* toString(JobStatus status) {
    switch (status) {
        case JobStatus::Created:
            return "Created";
        case JobStatus::Running:
            return "Running";
        case JobStatus::Succeeded:
            return "Succeeded";
        case JobStatus::Failed:
            return "Failed";
        case JobStatus::Cancelled:
            return "Cancelled";
    }

    return "Unknown";
}

Task makeBenchmarkTask(TaskId taskId) {
    return Task{taskId, "task-" + std::to_string(taskId), [] {
        std::this_thread::sleep_for(kTaskWorkDuration);
    }};
}

Job buildIndependentJob(JobId jobId, std::size_t taskCount) {
    Job job{jobId, "independent"};
    for (std::size_t i = 0; i < taskCount; ++i) {
        job.addTask(makeBenchmarkTask(static_cast<TaskId>(i + 1)));
    }

    return job;
}

Job buildChainJob(JobId jobId, std::size_t taskCount) {
    Job job{jobId, "chain"};
    for (std::size_t i = 0; i < taskCount; ++i) {
        const auto taskId = static_cast<TaskId>(i + 1);
        job.addTask(makeBenchmarkTask(taskId));
        if (i > 0) {
            job.addDependency(taskId, static_cast<TaskId>(i));
        }
    }

    return job;
}

Job buildDiamondBatchJob(JobId jobId, std::size_t taskCount) {
    Job job{jobId, "diamond-batch"};
    TaskId nextTaskId = 1;

    const auto diamondCount = taskCount / 4;
    for (std::size_t i = 0; i < diamondCount; ++i) {
        const TaskId root = nextTaskId++;
        const TaskId left = nextTaskId++;
        const TaskId right = nextTaskId++;
        const TaskId join = nextTaskId++;

        job.addTask(makeBenchmarkTask(root));
        job.addTask(makeBenchmarkTask(left));
        job.addTask(makeBenchmarkTask(right));
        job.addTask(makeBenchmarkTask(join));

        job.addDependency(left, root);
        job.addDependency(right, root);
        job.addDependency(join, left);
        job.addDependency(join, right);
    }

    while (static_cast<std::size_t>(nextTaskId - 1) < taskCount) {
        job.addTask(makeBenchmarkTask(nextTaskId++));
    }

    return job;
}

Job buildBenchmarkJob(const std::string& scenario, JobId jobId, std::size_t taskCount) {
    if (scenario == "independent") {
        return buildIndependentJob(jobId, taskCount);
    }
    if (scenario == "chain") {
        return buildChainJob(jobId, taskCount);
    }
    if (scenario == "diamond_batch") {
        return buildDiamondBatchJob(jobId, taskCount);
    }

    throw std::runtime_error("unknown benchmark scenario: " + scenario);
}

int runBenchmark(const std::string& scenario, std::size_t taskCount, std::size_t workerCount) {
    auto repository = std::make_shared<InMemoryJobRepository>();
    JobScheduler scheduler{repository, workerCount};

    const JobId jobId = 1;
    auto job = buildBenchmarkJob(scenario, jobId, taskCount);

    const auto start = std::chrono::steady_clock::now();
    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(jobId);
    const auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    const auto finalStatus = scheduler.getJobStatus(jobId);
    scheduler.shutdown();

    const double totalSeconds = static_cast<double>(totalDuration.count()) / 1000.0;
    const double tasksPerSecond = totalSeconds > 0.0
        ? static_cast<double>(taskCount) / totalSeconds
        : static_cast<double>(taskCount);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "scenario=" << scenario
              << " task_count=" << taskCount
              << " worker_count=" << workerCount
              << " status=" << toString(finalStatus)
              << " total_ms=" << totalDuration.count()
              << " tasks_per_second=" << tasksPerSecond << '\n';

    return finalStatus == JobStatus::Succeeded ? 0 : 1;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc == 5 && std::string_view(argv[1]) == "--benchmark") {
            const std::string scenario = argv[2];
            const auto taskCount = static_cast<std::size_t>(std::stoull(argv[3]));
            const auto workerCount = static_cast<std::size_t>(std::stoull(argv[4]));

            if (taskCount == 0 || workerCount == 0) {
                throw std::runtime_error("task count and worker count must be greater than zero");
            }

            return runBenchmark(scenario, taskCount, workerCount);
        }

        std::cout << "Distributed Job Scheduler" << '\n';
        std::cout << "Benchmark usage: scheduler_app --benchmark <scenario> <task_count> <worker_count>" << '\n';
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << '\n';
        return 1;
    }
}
