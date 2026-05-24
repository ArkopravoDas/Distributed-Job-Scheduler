#include "scheduler/execution/TaskExecutor.hpp"

#include <chrono>
#include <exception>

namespace scheduler::execution {

ExecutionResult TaskExecutor::execute(scheduler::core::Task& task) {
    const auto startTime = std::chrono::steady_clock::now();

    try {
        task.execute();

        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime);
        if (task.timeout().count() > 0 && duration > task.timeout()) {
            return ExecutionResult{
                task.id(),
                false,
                "Task timed out",
                duration
            };
        }

        return ExecutionResult{
            task.id(),
            true,
            "",
            duration
        };
    } catch (const std::exception& ex) {
        return ExecutionResult{
            task.id(),
            false,
            ex.what(),
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime)
        };
    } catch (...) {
        return ExecutionResult{
            task.id(),
            false,
            "Unknown exception",
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime)
        };
    }
}

}  // namespace scheduler::execution
