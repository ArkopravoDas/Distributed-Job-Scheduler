#include "scheduler/core/Task.hpp"

#include <utility>

namespace scheduler::core {

Task::Task(TaskId id, std::string name, std::function<void()> action, std::size_t maxRetries)
    : id_(id),
      name_(std::move(name)),
      maxRetries_(maxRetries),
      action_(std::move(action)) {}

TaskId Task::id() const noexcept {
    return id_;
}

const std::string& Task::name() const noexcept {
    return name_;
}

TaskStatus Task::status() const noexcept {
    return status_;
}

void Task::setStatus(TaskStatus status) noexcept {
    status_ = status;
}

std::size_t Task::retryCount() const noexcept {
    return retryCount_;
}

std::size_t Task::maxRetries() const noexcept {
    return maxRetries_;
}

void Task::incrementRetryCount() noexcept {
    ++retryCount_;
}

void Task::execute() {
    status_ = TaskStatus::Running;

    try {
        if (action_) {
            action_();
        }

        status_ = TaskStatus::Succeeded;
    } catch (...) {
        status_ = TaskStatus::Failed;
        throw;
    }
}

}  // namespace scheduler::core
