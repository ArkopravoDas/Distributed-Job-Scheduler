#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "scheduler/scheduling/DependencyGraph.hpp"

namespace {

using scheduler::core::TaskId;
using scheduler::scheduling::DependencyGraph;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void expectEqual(const std::vector<TaskId>& actual,
                 const std::vector<TaskId>& expected,
                 const std::string& message) {
    expect(actual == expected, message);
}

void NoDependenciesAllReady() {
    DependencyGraph graph{{1, 2, 3}, {}};

    expect(!graph.hasCycle(), "graph without dependencies should not have a cycle");
    expectEqual(graph.getInitialReadyTasks(), {1, 2, 3},
                "all tasks should be ready when there are no dependencies");
}

void LinearDependency() {
    DependencyGraph graph{{1, 2, 3}, {{2, 1}, {3, 2}}};

    expectEqual(graph.getInitialReadyTasks(), {1},
                "only the root task should be initially ready");
    expectEqual(graph.markTaskCompleted(1), {2},
                "completing the first task should unlock the second");
    expectEqual(graph.markTaskCompleted(2), {3},
                "completing the second task should unlock the third");
}

void DiamondDependency() {
    DependencyGraph graph{{1, 2, 3, 4}, {{2, 1}, {3, 1}, {4, 2}, {4, 3}}};

    expectEqual(graph.getInitialReadyTasks(), {1},
                "only the prerequisite root should be initially ready");
    expectEqual(graph.markTaskCompleted(1), {2, 3},
                "completing the root should unlock both middle tasks");
    expectEqual(graph.markTaskCompleted(2), {},
                "task 4 should not be ready until all prerequisites are complete");
    expectEqual(graph.markTaskCompleted(3), {4},
                "completing the last prerequisite should unlock the dependent task");
}

void CycleDetected() {
    DependencyGraph graph{{1, 2, 3}, {{1, 3}, {2, 1}, {3, 2}}};

    expect(graph.hasCycle(), "cyclic dependencies should be detected");
    expectEqual(graph.getInitialReadyTasks(), {},
                "no task should be initially ready in a simple cycle");
}

void TaskReadyOnlyAfterAllPrerequisitesComplete() {
    DependencyGraph graph{{1, 2, 3}, {{3, 1}, {3, 2}}};

    expectEqual(graph.getInitialReadyTasks(), {1, 2},
                "tasks without prerequisites should start ready");
    expectEqual(graph.markTaskCompleted(1), {},
                "dependent task should stay blocked until all prerequisites finish");
    expect(!graph.isCompleted(3), "dependent task should not be marked complete early");
    expectEqual(graph.markTaskCompleted(2), {3},
                "dependent task should become ready after the final prerequisite completes");
}

void runTest(const char* name, void (*test)()) {
    test();
    std::cout << "[PASS] " << name << '\n';
}

}  // namespace

int main() {
    try {
        runTest("NoDependenciesAllReady", &NoDependenciesAllReady);
        runTest("LinearDependency", &LinearDependency);
        runTest("DiamondDependency", &DiamondDependency);
        runTest("CycleDetected", &CycleDetected);
        runTest("TaskReadyOnlyAfterAllPrerequisitesComplete",
                &TaskReadyOnlyAfterAllPrerequisitesComplete);
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
