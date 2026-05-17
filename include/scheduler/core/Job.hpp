#pragma once

#include <unordered_map>
#include <vector>
#include <string>

#include "scheduler/core/JobStatus.hpp"
#include "scheduler/core/SchedulerTypes.hpp"
#include "scheduler/core/Task.hpp"

namespace scheduler::core {

class Job {
public:
    using DependencyMap = std::unordered_map<TaskId, std::vector<TaskId>>;

    Job(JobId id, std::string name);

    JobId id() const noexcept;
    const std::string& name() const noexcept;
    JobStatus status() const noexcept;
    void setStatus(JobStatus status) noexcept;
    void addTask(Task task);
    Task* getTask(TaskId taskId) noexcept;
    const Task* getTask(TaskId taskId) const noexcept;
    std::vector<TaskId> getAllTaskIds() const;
    void addDependency(TaskId task, TaskId dependsOn);
    const DependencyMap& dependencies() const noexcept;

private:
    JobId id_;
    std::string name_;
    JobStatus status_{JobStatus::Created};
    std::unordered_map<TaskId, Task> tasks_;
    std::vector<TaskId> taskOrder_;
    DependencyMap dependencies_;
};

}  // namespace scheduler::core
