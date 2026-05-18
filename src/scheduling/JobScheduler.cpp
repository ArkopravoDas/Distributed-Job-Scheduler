#include "scheduler/scheduling/JobScheduler.hpp"

#include <optional>
#include <stdexcept>
#include <utility>

namespace scheduler::scheduling {

JobScheduler::JobScheduler(std::shared_ptr<scheduler::storage::JobRepository> repository,
                           std::size_t workerCount)
    : repository_(std::move(repository)),
      threadPool_(workerCount) {
    if (!repository_) {
        throw std::runtime_error("job repository is required");
    }
}

void JobScheduler::submitJob(scheduler::core::Job job) {
    const auto jobId = job.id();
    std::vector<scheduler::core::TaskId> readyTasks;
    std::optional<scheduler::core::Job> jobToSave;
    std::optional<scheduler::core::Job> jobToPersist;
    bool shouldNotify = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (shutdown_) {
            throw std::runtime_error("job scheduler is shut down");
        }

        if (jobs_.contains(jobId) || repository_->exists(jobId)) {
            throw std::runtime_error("job already exists");
        }

        auto state = std::make_shared<JobState>(JobState{
            std::move(job),
            DependencyGraph({}, {}),
            0,
            false
        });

        const auto taskIds = state->job.getAllTaskIds();
        state->dependencyGraph = DependencyGraph(taskIds, buildDependencies(state->job));
        state->remainingTasks = taskIds.size();
        jobs_.emplace(jobId, state);
        jobToSave = state->job;

        if (state->job.hasDuplicateTaskIds() || hasInvalidDependencies(state->job) ||
            state->dependencyGraph.hasCycle()) {
            state->job.setStatus(scheduler::core::JobStatus::Failed);
            state->terminal = true;
            jobToPersist = state->job;
            shouldNotify = true;
        } else if (state->remainingTasks == 0) {
            state->job.setStatus(scheduler::core::JobStatus::Succeeded);
            state->terminal = true;
            jobToPersist = state->job;
            shouldNotify = true;
        } else {
            state->job.setStatus(scheduler::core::JobStatus::Running);
            readyTasks = state->dependencyGraph.getInitialReadyTasks();
            for (const auto taskId : readyTasks) {
                if (auto* task = state->job.getTask(taskId)) {
                    task->setStatus(scheduler::core::TaskStatus::Ready);
                }
            }

            jobToPersist = state->job;
        }
    }

    if (jobToSave) {
        repository_->saveJob(*jobToSave);
    }
    if (jobToPersist) {
        repository_->updateJob(*jobToPersist);
    }
    if (shouldNotify) {
        condition_.notify_all();
        return;
    }

    scheduleTasks(jobId, readyTasks);
}

void JobScheduler::waitForJob(scheduler::core::JobId jobId) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (!jobs_.contains(jobId)) {
        throw std::runtime_error("job not found");
    }

    condition_.wait(lock, [&] {
        const auto state = getJobStateLocked(jobId);
        return state && state->terminal;
    });
}

scheduler::core::JobStatus JobScheduler::getJobStatus(scheduler::core::JobId jobId) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto state = getJobStateLocked(jobId);
    if (!state) {
        throw std::runtime_error("job not found");
    }

    return state->job.status();
}

scheduler::core::TaskStatus JobScheduler::getTaskStatus(scheduler::core::JobId jobId,
                                                        scheduler::core::TaskId taskId) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto state = getJobStateLocked(jobId);
    if (!state) {
        throw std::runtime_error("job not found");
    }

    const auto* task = state->job.getTask(taskId);
    if (!task) {
        throw std::runtime_error("task not found");
    }

    return task->status();
}

void JobScheduler::shutdown() {
    std::vector<scheduler::core::Job> jobsToPersist;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_) {
            return;
        }

        shutdown_ = true;
    }

    threadPool_.shutdown();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [jobId, state] : jobs_) {
            if (state->terminal) {
                continue;
            }

            for (const auto taskId : state->job.getAllTaskIds()) {
                auto* task = state->job.getTask(taskId);
                if (!task || isTerminalTaskStatus(task->status())) {
                    continue;
                }

                task->setStatus(scheduler::core::TaskStatus::Cancelled);
            }

            state->job.setStatus(scheduler::core::JobStatus::Cancelled);
            state->terminal = true;
            jobsToPersist.push_back(state->job);
        }
    }

    for (const auto& jobSnapshot : jobsToPersist) {
        repository_->updateJob(jobSnapshot);
    }

    condition_.notify_all();
}

bool JobScheduler::isTerminalStatus(scheduler::core::JobStatus status) {
    return status == scheduler::core::JobStatus::Succeeded ||
           status == scheduler::core::JobStatus::Failed ||
           status == scheduler::core::JobStatus::Cancelled;
}

bool JobScheduler::isTerminalTaskStatus(scheduler::core::TaskStatus status) {
    return status == scheduler::core::TaskStatus::Succeeded ||
           status == scheduler::core::TaskStatus::Failed ||
           status == scheduler::core::TaskStatus::Cancelled;
}

bool JobScheduler::hasInvalidDependencies(const scheduler::core::Job& job) const {
    for (const auto& [taskId, prerequisites] : job.dependencies()) {
        if (job.getTask(taskId) == nullptr) {
            return true;
        }

        for (const auto prerequisiteId : prerequisites) {
            if (job.getTask(prerequisiteId) == nullptr) {
                return true;
            }
        }
    }

    return false;
}

std::vector<DependencyGraph::Dependency> JobScheduler::buildDependencies(
    const scheduler::core::Job& job) const {
    std::vector<DependencyGraph::Dependency> dependencies;

    for (const auto& [taskId, prerequisites] : job.dependencies()) {
        for (const auto prerequisiteId : prerequisites) {
            dependencies.emplace_back(taskId, prerequisiteId);
        }
    }

    return dependencies;
}

void JobScheduler::scheduleTasks(scheduler::core::JobId jobId,
                                 const std::vector<scheduler::core::TaskId>& taskIds) {
    for (const auto taskId : taskIds) {
        try {
            threadPool_.submit([this, jobId, taskId] {
                executeScheduledTask(jobId, taskId);
            });
        } catch (const std::exception&) {
            std::optional<scheduler::core::Job> jobSnapshot;
            bool shouldNotify = false;

            {
                std::lock_guard<std::mutex> lock(mutex_);
                const auto state = getJobStateLocked(jobId);
                if (!state || state->terminal) {
                    continue;
                }

                state->job.setStatus(scheduler::core::JobStatus::Failed);
                state->terminal = true;
                jobSnapshot = state->job;
                shouldNotify = true;
            }

            if (jobSnapshot) {
                repository_->updateJob(*jobSnapshot);
            }
            if (shouldNotify) {
                condition_.notify_all();
            }
        }
    }
}

void JobScheduler::executeScheduledTask(scheduler::core::JobId jobId,
                                        scheduler::core::TaskId taskId) {
    scheduler::core::Task localTask{0, "", [] {}};
    std::optional<scheduler::core::Job> jobSnapshot;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto state = getJobStateLocked(jobId);
        if (!state) {
            return;
        }

        auto* task = state->job.getTask(taskId);
        if (!task) {
            return;
        }

        if (state->terminal && state->job.status() == scheduler::core::JobStatus::Failed) {
            return;
        }

        task->setStatus(scheduler::core::TaskStatus::Running);
        localTask = *task;
        jobSnapshot = state->job;
    }

    if (jobSnapshot) {
        repository_->updateJob(*jobSnapshot);
    }

    const auto result = taskExecutor_.execute(localTask);
    std::vector<scheduler::core::TaskId> newlyReadyTasks;
    std::optional<scheduler::core::Job> updatedJobSnapshot;
    bool shouldNotify = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto state = getJobStateLocked(jobId);
        if (!state) {
            return;
        }

        auto* task = state->job.getTask(taskId);
        if (!task) {
            return;
        }

        task->setStatus(result.success ? scheduler::core::TaskStatus::Succeeded
                                       : scheduler::core::TaskStatus::Failed);

        if (!result.success) {
            if (!state->terminal) {
                state->job.setStatus(scheduler::core::JobStatus::Failed);
                state->terminal = true;
            }

            updatedJobSnapshot = state->job;
            shouldNotify = true;
        } else if (state->terminal) {
            updatedJobSnapshot = state->job;
            shouldNotify = true;
        } else {
            newlyReadyTasks = state->dependencyGraph.markTaskCompleted(taskId);
            if (state->remainingTasks > 0) {
                --state->remainingTasks;
            }

            for (const auto readyTaskId : newlyReadyTasks) {
                if (auto* readyTask = state->job.getTask(readyTaskId)) {
                    readyTask->setStatus(scheduler::core::TaskStatus::Ready);
                }
            }

            if (state->remainingTasks == 0) {
                state->job.setStatus(scheduler::core::JobStatus::Succeeded);
                state->terminal = true;
            }

            updatedJobSnapshot = state->job;
            if (state->terminal || isTerminalStatus(state->job.status())) {
                shouldNotify = true;
            }
        }
    }

    if (updatedJobSnapshot) {
        repository_->updateJob(*updatedJobSnapshot);
    }
    if (shouldNotify) {
        condition_.notify_all();
    }

    if (!newlyReadyTasks.empty()) {
        scheduleTasks(jobId, newlyReadyTasks);
    }
}

std::shared_ptr<JobScheduler::JobState> JobScheduler::getJobStateLocked(
    scheduler::core::JobId jobId) {
    const auto it = jobs_.find(jobId);
    if (it == jobs_.end()) {
        return nullptr;
    }

    return it->second;
}

}  // namespace scheduler::scheduling
