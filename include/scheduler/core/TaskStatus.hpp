#pragma once

namespace scheduler::core {

enum class TaskStatus {
    Pending,
    Ready,
    Running,
    Succeeded,
    Failed,
    Cancelled
};

}  // namespace scheduler::core
