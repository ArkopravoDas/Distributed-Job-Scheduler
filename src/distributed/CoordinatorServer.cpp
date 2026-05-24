#include "scheduler/distributed/CoordinatorServer.hpp"

#include <optional>
#include <stdexcept>
#include <utility>

namespace scheduler::distributed {

void CoordinatorServer::submitJob(scheduler::core::Job job) {
    const auto jobId = job.id();

    std::lock_guard<std::mutex> lock(mutex_);
    if (jobs_.contains(jobId)) {
        throw std::runtime_error("job already exists");
    }

    auto state = std::make_shared<JobState>(JobState{
        std::move(job),
        scheduler::scheduling::DependencyGraph({}, {}),
        0,
        false
    });

    const auto taskIds = state->job.getAllTaskIds();
    state->dependencyGraph = scheduler::scheduling::DependencyGraph(
        taskIds,
        buildDependencies(state->job));
    state->remainingTasks = taskIds.size();
    jobs_.emplace(jobId, state);

    if (state->job.hasDuplicateTaskIds() || hasInvalidDependencies(state->job) ||
        state->dependencyGraph.hasCycle()) {
        state->job.setStatus(scheduler::core::JobStatus::Failed);
        state->terminal = true;
    } else if (state->remainingTasks == 0) {
        state->job.setStatus(scheduler::core::JobStatus::Succeeded);
        state->terminal = true;
    } else {
        state->job.setStatus(scheduler::core::JobStatus::Running);
        const auto readyTasks = state->dependencyGraph.getInitialReadyTasks();
        for (const auto taskId : readyTasks) {
            if (auto* task = state->job.getTask(taskId)) {
                task->setStatus(scheduler::core::TaskStatus::Ready);
            }
        }
    }

    if (state->terminal) {
        condition_.notify_all();
    }
}

void CoordinatorServer::waitForJob(scheduler::core::JobId jobId) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!jobs_.contains(jobId)) {
        throw std::runtime_error("job not found");
    }

    condition_.wait(lock, [&] {
        const auto state = getJobStateLocked(jobId);
        return state && state->terminal;
    });
}

scheduler::core::JobStatus CoordinatorServer::getJobStatus(scheduler::core::JobId jobId) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto state = getJobStateLocked(jobId);
    if (!state) {
        throw std::runtime_error("job not found");
    }

    return state->job.status();
}

scheduler::core::TaskStatus CoordinatorServer::getTaskStatus(
    scheduler::core::JobId jobId,
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

std::string CoordinatorServer::handleRawMessage(const std::string& rawMessage) {
    const auto parsed = Message::parse(rawMessage);
    if (!parsed.has_value()) {
        return Message{MessageType::Error, "", std::nullopt, std::nullopt, "", std::nullopt,
                       std::nullopt, "invalid_message", ""}
            .serialize();
    }

    const auto& message = *parsed;
    if (message.workerId.empty()) {
        return Message{MessageType::Error, "", std::nullopt, std::nullopt, "", std::nullopt,
                       std::nullopt, "missing_worker_id", ""}
            .serialize();
    }

    std::lock_guard<std::mutex> lock(mutex_);

    switch (message.type) {
        case MessageType::RegisterWorker:
            registry_.registerWorker(message.workerId);
            return Message{MessageType::Ack, message.workerId, std::nullopt, std::nullopt, "",
                           std::nullopt, std::nullopt, "", "registered"}
                .serialize();
        case MessageType::Heartbeat:
            if (!registry_.recordHeartbeat(message.workerId)) {
                return Message{MessageType::Error, message.workerId, std::nullopt, std::nullopt,
                               "", std::nullopt, std::nullopt, "unknown_worker", ""}
                    .serialize();
            }

            return Message{MessageType::Ack, message.workerId, std::nullopt, std::nullopt, "",
                           std::nullopt, std::nullopt, "", "heartbeat_ok"}
                .serialize();
        case MessageType::RequestTask:
            if (!registry_.exists(message.workerId) || !registry_.isWorkerAlive(message.workerId)) {
                return Message{MessageType::Error, message.workerId, std::nullopt, std::nullopt,
                               "", std::nullopt, std::nullopt, "worker_not_available", ""}
                    .serialize();
            }

            return assignTaskToWorkerLocked(message.workerId);
        case MessageType::TaskResult:
            return handleTaskResultLocked(message);
        case MessageType::AssignTask:
        case MessageType::Ack:
        case MessageType::Error:
        case MessageType::Unknown:
            return Message{MessageType::Error, message.workerId, std::nullopt, std::nullopt, "",
                           std::nullopt, std::nullopt, "unsupported_message", ""}
                .serialize();
    }

    return Message{MessageType::Error, message.workerId, std::nullopt, std::nullopt, "",
                   std::nullopt, std::nullopt, "unsupported_message", ""}
        .serialize();
}

std::vector<std::string> CoordinatorServer::markDeadWorkers(std::chrono::milliseconds timeout) {
    const auto deadWorkers = registry_.markDeadWorkers(timeout);
    if (deadWorkers.empty()) {
        return deadWorkers;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& workerId : deadWorkers) {
        const auto assignmentIt = assignmentsByWorker_.find(workerId);
        if (assignmentIt == assignmentsByWorker_.end()) {
            continue;
        }

        const auto assignment = assignmentIt->second;
        assignmentsByWorker_.erase(assignmentIt);

        const auto state = getJobStateLocked(assignment.jobId);
        if (!state || state->terminal) {
            continue;
        }

        auto* task = state->job.getTask(assignment.taskId);
        if (!task || task->status() != scheduler::core::TaskStatus::Running) {
            continue;
        }

        // We do not kill the worker thread; we simply make the task eligible for reassignment.
        task->setStatus(scheduler::core::TaskStatus::Ready);
    }

    return deadWorkers;
}

const WorkerRegistry& CoordinatorServer::registry() const noexcept {
    return registry_;
}

bool CoordinatorServer::isTerminalStatus(scheduler::core::JobStatus status) {
    return status == scheduler::core::JobStatus::Succeeded ||
           status == scheduler::core::JobStatus::Failed ||
           status == scheduler::core::JobStatus::Cancelled;
}

bool CoordinatorServer::hasInvalidDependencies(const scheduler::core::Job& job) const {
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

std::vector<scheduler::scheduling::DependencyGraph::Dependency>
CoordinatorServer::buildDependencies(const scheduler::core::Job& job) const {
    std::vector<scheduler::scheduling::DependencyGraph::Dependency> dependencies;

    for (const auto& [taskId, prerequisites] : job.dependencies()) {
        for (const auto prerequisiteId : prerequisites) {
            dependencies.emplace_back(taskId, prerequisiteId);
        }
    }

    return dependencies;
}

std::shared_ptr<CoordinatorServer::JobState> CoordinatorServer::getJobStateLocked(
    scheduler::core::JobId jobId) {
    const auto it = jobs_.find(jobId);
    if (it == jobs_.end()) {
        return nullptr;
    }

    return it->second;
}

std::string CoordinatorServer::assignTaskToWorkerLocked(const std::string& workerId) {
    if (assignmentsByWorker_.contains(workerId)) {
        return Message{MessageType::Ack, workerId, std::nullopt, std::nullopt, "", std::nullopt,
                       std::nullopt, "", "already_busy"}
            .serialize();
    }

    for (auto& [jobId, state] : jobs_) {
        if (state->terminal || state->job.status() != scheduler::core::JobStatus::Running) {
            continue;
        }

        for (const auto taskId : state->job.getAllTaskIds()) {
            auto* task = state->job.getTask(taskId);
            if (!task || task->status() != scheduler::core::TaskStatus::Ready) {
                continue;
            }

            task->setStatus(scheduler::core::TaskStatus::Running);
            assignmentsByWorker_[workerId] = Assignment{workerId, jobId, taskId};

            const auto timeoutMs = static_cast<std::uint64_t>(task->timeout().count());
            return Message{
                MessageType::AssignTask,
                workerId,
                jobId,
                taskId,
                task->name(),
                std::nullopt,
                timeoutMs,
                "",
                ""
            }.serialize();
        }
    }

    return Message{MessageType::Ack, workerId, std::nullopt, std::nullopt, "", std::nullopt,
                   std::nullopt, "", "no_task"}
        .serialize();
}

std::string CoordinatorServer::handleTaskResultLocked(const Message& message) {
    if (!message.jobId.has_value() || !message.taskId.has_value() || !message.success.has_value()) {
        return Message{MessageType::Error, message.workerId, std::nullopt, std::nullopt, "",
                       std::nullopt, std::nullopt, "missing_result_fields", ""}
            .serialize();
    }

    const auto assignmentIt = assignmentsByWorker_.find(message.workerId);
    if (assignmentIt == assignmentsByWorker_.end()) {
        return Message{MessageType::Error, message.workerId, *message.jobId, *message.taskId, "",
                       std::nullopt, std::nullopt, "stale_assignment", ""}
            .serialize();
    }

    const auto assignment = assignmentIt->second;
    if (assignment.jobId != *message.jobId || assignment.taskId != *message.taskId) {
        return Message{MessageType::Error, message.workerId, *message.jobId, *message.taskId, "",
                       std::nullopt, std::nullopt, "assignment_mismatch", ""}
            .serialize();
    }

    assignmentsByWorker_.erase(assignmentIt);

    const auto state = getJobStateLocked(*message.jobId);
    if (!state) {
        return Message{MessageType::Error, message.workerId, *message.jobId, *message.taskId, "",
                       std::nullopt, std::nullopt, "job_not_found", ""}
            .serialize();
    }

    auto* task = state->job.getTask(*message.taskId);
    if (!task) {
        return Message{MessageType::Error, message.workerId, *message.jobId, *message.taskId, "",
                       std::nullopt, std::nullopt, "task_not_found", ""}
            .serialize();
    }

    if (!*message.success && task->retryCount() < task->maxRetries() && !state->terminal) {
        task->incrementRetryCount();
        task->setStatus(scheduler::core::TaskStatus::Ready);
    } else if (!*message.success) {
        task->setStatus(scheduler::core::TaskStatus::Failed);
        if (!state->terminal) {
            state->job.setStatus(scheduler::core::JobStatus::Failed);
            state->terminal = true;
            condition_.notify_all();
        }
    } else if (state->terminal) {
        task->setStatus(scheduler::core::TaskStatus::Succeeded);
        condition_.notify_all();
    } else {
        task->setStatus(scheduler::core::TaskStatus::Succeeded);
        const auto newlyReadyTasks = state->dependencyGraph.markTaskCompleted(*message.taskId);
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
            condition_.notify_all();
        }
    }

    return Message{MessageType::Ack, message.workerId, *message.jobId, *message.taskId, "",
                   std::nullopt, std::nullopt, "", "result_recorded"}
        .serialize();
}

}  // namespace scheduler::distributed
