#pragma once

namespace scheduler::core {

enum class JobStatus {
    Created,
    Running,
    Succeeded,
    Failed,
    Cancelled
};

}  // namespace scheduler::core
