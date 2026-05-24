#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace scheduler::distributed {

struct WorkerInfo {
    std::string workerId;
    std::chrono::steady_clock::time_point lastHeartbeat;
    bool alive{true};
};

class WorkerRegistry {
public:
    void registerWorker(const std::string& workerId);
    bool recordHeartbeat(const std::string& workerId);
    std::vector<std::string> markDeadWorkers(std::chrono::milliseconds timeout);
    bool isWorkerAlive(const std::string& workerId) const;
    bool exists(const std::string& workerId) const;
    std::optional<WorkerInfo> getWorker(const std::string& workerId) const;
    std::vector<WorkerInfo> listWorkers() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, WorkerInfo> workers_;
};

}  // namespace scheduler::distributed
