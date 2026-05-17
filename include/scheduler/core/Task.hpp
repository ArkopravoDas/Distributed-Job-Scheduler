#pragma once

#include <cstddef>
#include <functional>
#include <string>

#include "scheduler/core/SchedulerTypes.hpp"
#include "scheduler/core/TaskStatus.hpp"

namespace scheduler::core {

class Task {
public:
    Task(TaskId id, std::string name, std::function<void()> action, std::size_t maxRetries = 0);

    TaskId id() const noexcept;
    const std::string& name() const noexcept;
    TaskStatus status() const noexcept;
    void setStatus(TaskStatus status) noexcept;
    std::size_t retryCount() const noexcept;
    std::size_t maxRetries() const noexcept;
    void incrementRetryCount() noexcept;
    void execute();

private:
    TaskId id_;
    std::string name_;
    TaskStatus status_{TaskStatus::Pending};
    std::size_t retryCount_{0};
    std::size_t maxRetries_{0};
    std::function<void()> action_;
};

}  // namespace scheduler::core
