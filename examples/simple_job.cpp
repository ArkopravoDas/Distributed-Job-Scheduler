#include <iostream>
#include <memory>

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

}  // namespace

int main() {
    auto repository = std::make_shared<scheduler::storage::InMemoryJobRepository>();
    scheduler::scheduling::JobScheduler scheduler{repository, 2};

    scheduler::core::Job job{1, "simple-job"};
    job.addTask(scheduler::core::Task{1, "task-1", [] {
        std::cout << "task-1 executed\n";
    }});
    job.addTask(scheduler::core::Task{2, "task-2", [] {
        std::cout << "task-2 executed\n";
    }});
    job.addTask(scheduler::core::Task{3, "task-3", [] {
        std::cout << "task-3 executed\n";
    }});

    scheduler.submitJob(std::move(job));
    scheduler.waitForJob(1);

    std::cout << "Final job status: " << toString(scheduler.getJobStatus(1)) << '\n';
    scheduler.shutdown();
    return 0;
}
