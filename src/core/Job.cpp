#include "scheduler/core/Job.hpp"

#include <utility>

namespace scheduler::core {

Job::Job(JobId id, std::string name)
    : id_(id), name_(std::move(name)) {}

JobId Job::id() const noexcept {
    return id_;
}

const std::string& Job::name() const noexcept {
    return name_;
}

JobStatus Job::status() const noexcept {
    return status_;
}

void Job::setStatus(JobStatus status) noexcept {
    status_ = status;
}

void Job::addTask(Task task) {
    const auto taskId = task.id();

    auto [it, inserted] = tasks_.insert_or_assign(taskId, std::move(task));
    if (inserted) {
        taskOrder_.push_back(it->first);
    } else {
        hasDuplicateTaskIds_ = true;
    }
}

Task* Job::getTask(TaskId taskId) noexcept {
    const auto it = tasks_.find(taskId);
    if (it == tasks_.end()) {
        return nullptr;
    }

    return &it->second;
}

const Task* Job::getTask(TaskId taskId) const noexcept {
    const auto it = tasks_.find(taskId);
    if (it == tasks_.end()) {
        return nullptr;
    }

    return &it->second;
}

std::vector<TaskId> Job::getAllTaskIds() const {
    return taskOrder_;
}

void Job::addDependency(TaskId task, TaskId dependsOn) {
    dependencies_[task].push_back(dependsOn);
}

const Job::DependencyMap& Job::dependencies() const noexcept {
    return dependencies_;
}

bool Job::hasDuplicateTaskIds() const noexcept {
    return hasDuplicateTaskIds_;
}

}  // namespace scheduler::core
