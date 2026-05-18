#pragma once

#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "scheduler/core/SchedulerTypes.hpp"

namespace scheduler::scheduling {

class DependencyGraph {
public:
    using Dependency = std::pair<scheduler::core::TaskId, scheduler::core::TaskId>;

    DependencyGraph(std::vector<scheduler::core::TaskId> taskIds,
                    std::vector<Dependency> dependencies);

    bool hasCycle() const;
    std::vector<scheduler::core::TaskId> getInitialReadyTasks() const;
    std::vector<scheduler::core::TaskId> markTaskCompleted(scheduler::core::TaskId taskId);
    bool isCompleted(scheduler::core::TaskId taskId) const;

private:
    std::vector<scheduler::core::TaskId> sortedReadyTasks(
        const std::vector<scheduler::core::TaskId>& taskIds) const;

    std::unordered_map<scheduler::core::TaskId, std::vector<scheduler::core::TaskId>> adjacency_;
    std::unordered_map<scheduler::core::TaskId, std::size_t> indegree_;
    std::unordered_set<scheduler::core::TaskId> completed_;
};

}  // namespace scheduler::scheduling
