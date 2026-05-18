#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

#include "scheduler/core/Job.hpp"
#include "scheduler/core/JobStatus.hpp"
#include "scheduler/core/Task.hpp"
#include "scheduler/scheduling/JobScheduler.hpp"
#include "scheduler/storage/InMemoryJobRepository.hpp"

namespace {

const char* toString(scheduler::core::JobStatus status) {
    switch (status) {
        case scheduler::core::JobStatus::Created:
            return "Created";
        case scheduler::core::JobStatus::Running:
            return "Running";
        case scheduler::core::JobStatus::Succeeded:
            return "Succeeded";
        case scheduler::core::JobStatus::Failed:
            return "Failed";
        case scheduler::core::JobStatus::Cancelled:
            return "Cancelled";
    }

    return "Unknown";
}

void logLine(std::mutex& mutex, const char* message) {
    std::lock_guard<std::mutex> lock(mutex);
    std::cout << message << '\n';
}

}  // namespace

int main() {
    auto repository = std::make_shared<scheduler::storage::InMemoryJobRepository>();
    scheduler::scheduling::JobScheduler scheduler{repository, 2};
    std::mutex logMutex;

    scheduler::core::Job job{2, "dependent-job"};
    job.addTask(scheduler::core::Task{1, "task-1", [&] {
        logLine(logMutex, "task-1 started");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        logLine(logMutex, "task-1 finished");
    }});
    job.addTask(scheduler::core::Task{2, "task-2", [&] {
        logLine(logMutex, "task-2 started");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        logLine(logMutex, "task-2 finished");
    }});
    job.addTask(scheduler::core::Task{3, "task-3", [&] {
        logLine(logMutex, "task-3 started");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        logLine(logMutex, "task-3 finished");
    }});
    job.addTask(scheduler::core::Task{4, "task-4", [&] {
        logLine(logMutex, "task-4 started");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        logLine(logMutex, "task-4 finished");
    }});

    job.addDependency(2, 1);
    job.addDependency(3, 1);
    job.addDependency(4, 2);
    job.addDependency(4, 3);

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(2);

    std::cout << "Final job status: " << toString(scheduler.getJobStatus(2)) << '\n';
    scheduler.shutdown();
    return 0;
}
