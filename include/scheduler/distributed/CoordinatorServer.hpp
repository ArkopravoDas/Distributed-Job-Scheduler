#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "scheduler/core/Job.hpp"
#include "scheduler/core/JobStatus.hpp"
#include "scheduler/core/TaskStatus.hpp"
#include "scheduler/distributed/Message.hpp"
#include "scheduler/distributed/WorkerRegistry.hpp"
#include "scheduler/scheduling/DependencyGraph.hpp"

namespace scheduler::distributed {

class CoordinatorServer {
public:
    void submitJob(scheduler::core::Job job);
    void waitForJob(scheduler::core::JobId jobId);
    scheduler::core::JobStatus getJobStatus(scheduler::core::JobId jobId);
    scheduler::core::TaskStatus getTaskStatus(scheduler::core::JobId jobId,
                                              scheduler::core::TaskId taskId);
    std::string handleRawMessage(const std::string& rawMessage);
    std::vector<std::string> markDeadWorkers(std::chrono::milliseconds timeout);
    const WorkerRegistry& registry() const noexcept;

private:
    struct JobState {
        scheduler::core::Job job;
        scheduler::scheduling::DependencyGraph dependencyGraph;
        std::size_t remainingTasks;
        bool terminal{false};
    };

    struct Assignment {
        std::string workerId;
        scheduler::core::JobId jobId;
        scheduler::core::TaskId taskId;
    };

    static bool isTerminalStatus(scheduler::core::JobStatus status);
    bool hasInvalidDependencies(const scheduler::core::Job& job) const;
    std::vector<scheduler::scheduling::DependencyGraph::Dependency> buildDependencies(
        const scheduler::core::Job& job) const;
    std::shared_ptr<JobState> getJobStateLocked(scheduler::core::JobId jobId);
    std::string assignTaskToWorkerLocked(const std::string& workerId);
    std::string handleTaskResultLocked(const Message& message);

    WorkerRegistry registry_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::unordered_map<scheduler::core::JobId, std::shared_ptr<JobState>> jobs_;
    std::unordered_map<std::string, Assignment> assignmentsByWorker_;
};

}  // namespace scheduler::distributed
