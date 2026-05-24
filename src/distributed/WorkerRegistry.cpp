#include "scheduler/distributed/WorkerRegistry.hpp"

namespace scheduler::distributed {

void WorkerRegistry::registerWorker(const std::string& workerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    workers_[workerId] = WorkerInfo{workerId, std::chrono::steady_clock::now(), true};
}

bool WorkerRegistry::recordHeartbeat(const std::string& workerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = workers_.find(workerId);
    if (it == workers_.end()) {
        return false;
    }

    it->second.lastHeartbeat = std::chrono::steady_clock::now();
    it->second.alive = true;
    return true;
}

std::vector<std::string> WorkerRegistry::markDeadWorkers(std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    std::vector<std::string> deadWorkers;

    for (auto& [workerId, workerInfo] : workers_) {
        if (workerInfo.alive && (now - workerInfo.lastHeartbeat) > timeout) {
            workerInfo.alive = false;
            deadWorkers.push_back(workerId);
        }
    }

    return deadWorkers;
}

bool WorkerRegistry::isWorkerAlive(const std::string& workerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = workers_.find(workerId);
    return it != workers_.end() && it->second.alive;
}

bool WorkerRegistry::exists(const std::string& workerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return workers_.contains(workerId);
}

std::optional<WorkerInfo> WorkerRegistry::getWorker(const std::string& workerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = workers_.find(workerId);
    if (it == workers_.end()) {
        return std::nullopt;
    }

    return it->second;
}

std::vector<WorkerInfo> WorkerRegistry::listWorkers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<WorkerInfo> workers;
    workers.reserve(workers_.size());

    for (const auto& [workerId, workerInfo] : workers_) {
        workers.push_back(workerInfo);
    }

    return workers;
}

}  // namespace scheduler::distributed
