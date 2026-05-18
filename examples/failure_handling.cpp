#include <iostream>
#include <memory>
#include <stdexcept>

#include "scheduler/core/Job.hpp"
#include "scheduler/core/JobStatus.hpp"
#include "scheduler/core/Task.hpp"
#include "scheduler/core/TaskStatus.hpp"
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

}  // namespace

int main() {
    auto repository = std::make_shared<scheduler::storage::InMemoryJobRepository>();
    scheduler::scheduling::JobScheduler scheduler{repository, 2};

    scheduler::core::Job job{3, "failure-job"};
    job.addTask(scheduler::core::Task{1, "task-1", [] {
        std::cout << "task-1 about to throw\n";
        throw std::runtime_error("task-1 failed");
    }});
    job.addTask(scheduler::core::Task{2, "task-2", [] {
        std::cout << "task-2 should not run after failure in dependent flow\n";
    }});
    job.addDependency(2, 1);

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(3);

    std::cout << "Final job status: " << toString(scheduler.getJobStatus(3)) << '\n';
    scheduler.shutdown();
    return 0;
}
