#include <chrono>
#include <atomic>
#include <iostream>
#include <thread>

#include "scheduler/core/Job.hpp"
#include "scheduler/core/Task.hpp"
#include "scheduler/distributed/CoordinatorServer.hpp"
#include "scheduler/distributed/WorkerClient.hpp"

int main() {
    scheduler::distributed::CoordinatorServer coordinator;
    std::atomic<int> completedTasks{0};

    scheduler::distributed::WorkerClient worker{
        "worker-1",
        [&](const std::string& message) {
            return coordinator.handleRawMessage(message);
        }
    };

    worker.registerTaskHandler("demo_task", [&] {
        std::this_thread::sleep_for(std::chrono::milliseconds{200});
        completedTasks.fetch_add(1);
    });
    worker.registerWithCoordinator();
    worker.startHeartbeatLoop();
    worker.startWorkLoop();

    scheduler::core::Job job{1, "demo-job"};
    job.addTask(scheduler::core::Task{1, "demo_task", [] {}});
    coordinator.submitJob(std::move(job));

    coordinator.waitForJob(1);
    worker.stopWorkLoop();
    worker.stopHeartbeatLoop();

    std::cout << "Worker completed tasks: " << completedTasks.load() << '\n';
    std::cout << "Final job status: "
              << (coordinator.getJobStatus(1) == scheduler::core::JobStatus::Succeeded
                      ? "Succeeded"
                      : "NotSucceeded")
              << '\n';
    return 0;
}
