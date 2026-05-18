#include "scheduler/scheduling/DependencyGraph.hpp"

#include <algorithm>
#include <queue>

namespace scheduler::scheduling {

DependencyGraph::DependencyGraph(std::vector<scheduler::core::TaskId> taskIds,
                                 std::vector<Dependency> dependencies) {
    for (const auto taskId : taskIds) {
        indegree_.try_emplace(taskId, 0);
        adjacency_.try_emplace(taskId, std::vector<scheduler::core::TaskId>{});
    }

    for (const auto& [task, dependsOn] : dependencies) {
        indegree_.try_emplace(task, 0);
        indegree_.try_emplace(dependsOn, 0);
        adjacency_.try_emplace(task, std::vector<scheduler::core::TaskId>{});
        adjacency_.try_emplace(dependsOn, std::vector<scheduler::core::TaskId>{});

        adjacency_[dependsOn].push_back(task);
        ++indegree_[task];
    }
}

bool DependencyGraph::hasCycle() const {
    auto indegreeCopy = indegree_;
    std::queue<scheduler::core::TaskId> ready;

    for (const auto& [taskId, count] : indegreeCopy) {
        if (count == 0) {
            ready.push(taskId);
        }
    }

    std::size_t visitedCount = 0;
    while (!ready.empty()) {
        const auto taskId = ready.front();
        ready.pop();
        ++visitedCount;

        const auto adjacencyIt = adjacency_.find(taskId);
        if (adjacencyIt == adjacency_.end()) {
            continue;
        }

        for (const auto dependentTaskId : adjacencyIt->second) {
            auto& remaining = indegreeCopy[dependentTaskId];
            --remaining;
            if (remaining == 0) {
                ready.push(dependentTaskId);
            }
        }
    }

    return visitedCount != indegreeCopy.size();
}

std::vector<scheduler::core::TaskId> DependencyGraph::getInitialReadyTasks() const {
    std::vector<scheduler::core::TaskId> readyTasks;

    for (const auto& [taskId, count] : indegree_) {
        if (count == 0 && !completed_.contains(taskId)) {
            readyTasks.push_back(taskId);
        }
    }

    return sortedReadyTasks(readyTasks);
}

std::vector<scheduler::core::TaskId> DependencyGraph::markTaskCompleted(
    scheduler::core::TaskId taskId) {
    if (completed_.contains(taskId)) {
        return {};
    }

    completed_.insert(taskId);

    std::vector<scheduler::core::TaskId> readyTasks;
    const auto adjacencyIt = adjacency_.find(taskId);
    if (adjacencyIt == adjacency_.end()) {
        return readyTasks;
    }

    for (const auto dependentTaskId : adjacencyIt->second) {
        auto indegreeIt = indegree_.find(dependentTaskId);
        if (indegreeIt == indegree_.end() || indegreeIt->second == 0) {
            continue;
        }

        --indegreeIt->second;
        if (indegreeIt->second == 0 && !completed_.contains(dependentTaskId)) {
            readyTasks.push_back(dependentTaskId);
        }
    }

    return sortedReadyTasks(readyTasks);
}

bool DependencyGraph::isCompleted(scheduler::core::TaskId taskId) const {
    return completed_.contains(taskId);
}

std::vector<scheduler::core::TaskId> DependencyGraph::sortedReadyTasks(
    const std::vector<scheduler::core::TaskId>& taskIds) const {
    auto sorted = taskIds;
    std::sort(sorted.begin(), sorted.end());
    return sorted;
}

}  // namespace scheduler::scheduling
