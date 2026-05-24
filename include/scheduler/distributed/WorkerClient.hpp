#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <thread>

#include "scheduler/execution/TaskExecutor.hpp"

namespace scheduler::distributed {

class WorkerClient {
public:
    using Transport = std::function<std::string(const std::string&)>;
    using TaskHandler = std::function<void()>;

    WorkerClient(std::string workerId, Transport transport);
    ~WorkerClient();

    void registerTaskHandler(std::string taskName, TaskHandler handler);
    void registerWithCoordinator();
    void startHeartbeatLoop();
    void stopHeartbeatLoop();
    void startWorkLoop();
    void stopWorkLoop();

private:
    void sendHeartbeat();
    void heartbeatLoop();
    void workLoop();
    void pollForTask();
    static std::string normalizeError(std::string message);

    std::string workerId_;
    Transport transport_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::thread heartbeatThread_;
    std::thread workThread_;
    std::unordered_map<std::string, TaskHandler> taskHandlers_;
    scheduler::execution::TaskExecutor taskExecutor_;
    bool heartbeatRunning_{false};
    bool workRunning_{false};
};

}  // namespace scheduler::distributed
