#include "scheduler/execution/TaskExecutor.hpp"

#include <chrono>
#include <exception>

namespace scheduler::execution {

ExecutionResult TaskExecutor::execute(scheduler::core::Task& task) {
    const auto startTime = std::chrono::steady_clock::now();

    try {
        task.execute();

        return ExecutionResult{
            task.id(),
            true,
            "",
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime)
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
