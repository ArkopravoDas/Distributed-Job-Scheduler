#pragma once

#include <chrono>
#include <string>

#include "scheduler/core/SchedulerTypes.hpp"

namespace scheduler::execution {

struct ExecutionResult {
    scheduler::core::TaskId taskId;
    bool success;
    std::string errorMessage;
    std::chrono::milliseconds duration;
};

}  // namespace scheduler::execution
