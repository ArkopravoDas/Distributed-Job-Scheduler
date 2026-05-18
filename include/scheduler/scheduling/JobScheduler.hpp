#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "scheduler/concurrency/ThreadPool.hpp"
#include "scheduler/core/Job.hpp"
#include "scheduler/core/JobStatus.hpp"
#include "scheduler/core/TaskStatus.hpp"
#include "scheduler/execution/TaskExecutor.hpp"
#include "scheduler/scheduling/DependencyGraph.hpp"
#include "scheduler/storage/JobRepository.hpp"

namespace scheduler::scheduling {

class JobScheduler {
public:
    JobScheduler(std::shared_ptr<scheduler::storage::JobRepository> repository,
                 std::size_t workerCount);

    void submitJob(scheduler::core::Job job);
    void waitForJob(scheduler::core::JobId jobId);
    scheduler::core::JobStatus getJobStatus(scheduler::core::JobId jobId);
    scheduler::core::TaskStatus getTaskStatus(scheduler::core::JobId jobId,
                                              scheduler::core::TaskId taskId);
    void shutdown();

private:
    struct JobState {
        scheduler::core::Job job;
        DependencyGraph dependencyGraph;
        std::size_t remainingTasks;
        bool terminal{false};
    };

    static bool isTerminalStatus(scheduler::core::JobStatus status);
    static bool isTerminalTaskStatus(scheduler::core::TaskStatus status);
    bool hasInvalidDependencies(const scheduler::core::Job& job) const;

    std::vector<DependencyGraph::Dependency> buildDependencies(
        const scheduler::core::Job& job) const;
    void scheduleTasks(scheduler::core::JobId jobId,
                       const std::vector<scheduler::core::TaskId>& taskIds);
    void executeScheduledTask(scheduler::core::JobId jobId, scheduler::core::TaskId taskId);
    std::shared_ptr<JobState> getJobStateLocked(scheduler::core::JobId jobId);

    std::shared_ptr<scheduler::storage::JobRepository> repository_;
    scheduler::concurrency::ThreadPool threadPool_;
    scheduler::execution::TaskExecutor taskExecutor_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::unordered_map<scheduler::core::JobId, std::shared_ptr<JobState>> jobs_;
    bool shutdown_{false};
};

}  // namespace scheduler::scheduling
