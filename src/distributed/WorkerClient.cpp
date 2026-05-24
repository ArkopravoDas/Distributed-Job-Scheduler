#include "scheduler/distributed/WorkerClient.hpp"

#include <chrono>
#include <cctype>
#include <stdexcept>
#include <utility>

#include "scheduler/core/Task.hpp"
#include "scheduler/distributed/Message.hpp"

namespace scheduler::distributed {

WorkerClient::WorkerClient(std::string workerId, Transport transport)
    : workerId_(std::move(workerId)),
      transport_(std::move(transport)) {
    if (workerId_.empty()) {
        throw std::runtime_error("worker id is required");
    }
    if (!transport_) {
        throw std::runtime_error("worker transport is required");
    }
}

WorkerClient::~WorkerClient() {
    stopWorkLoop();
    stopHeartbeatLoop();
}

void WorkerClient::registerTaskHandler(std::string taskName, TaskHandler handler) {
    if (taskName.empty()) {
        throw std::runtime_error("task name is required");
    }
    if (!handler) {
        throw std::runtime_error("task handler is required");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    taskHandlers_[std::move(taskName)] = std::move(handler);
}

void WorkerClient::registerWithCoordinator() {
    const auto response = transport_(
        Message{MessageType::RegisterWorker, workerId_}.serialize());
    const auto parsed = Message::parse(response);
    if (!parsed.has_value() || parsed->type != MessageType::Ack) {
        throw std::runtime_error("worker registration failed");
    }
}

void WorkerClient::startHeartbeatLoop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (heartbeatRunning_) {
        return;
    }

    heartbeatRunning_ = true;
    heartbeatThread_ = std::thread(&WorkerClient::heartbeatLoop, this);
}

void WorkerClient::stopHeartbeatLoop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!heartbeatRunning_) {
            return;
        }

        heartbeatRunning_ = false;
    }

    condition_.notify_all();

    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }
}

void WorkerClient::startWorkLoop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (workRunning_) {
        return;
    }

    workRunning_ = true;
    workThread_ = std::thread(&WorkerClient::workLoop, this);
}

void WorkerClient::stopWorkLoop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!workRunning_) {
            return;
        }

        workRunning_ = false;
    }

    condition_.notify_all();

    if (workThread_.joinable()) {
        workThread_.join();
    }
}

void WorkerClient::sendHeartbeat() {
    const auto response = transport_(
        Message{MessageType::Heartbeat, workerId_}.serialize());
    const auto parsed = Message::parse(response);
    if (!parsed.has_value() || parsed->type != MessageType::Ack) {
        throw std::runtime_error("worker heartbeat failed");
    }
}

void WorkerClient::heartbeatLoop() {
    std::unique_lock<std::mutex> lock(mutex_);

    while (heartbeatRunning_) {
        if (condition_.wait_for(lock, std::chrono::seconds(2), [this] {
                return !heartbeatRunning_;
            })) {
            break;
        }

        lock.unlock();
        try {
            sendHeartbeat();
        } catch (...) {
        }
        lock.lock();
    }
}

void WorkerClient::workLoop() {
    std::unique_lock<std::mutex> lock(mutex_);

    while (workRunning_) {
        if (condition_.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !workRunning_;
            })) {
            break;
        }

        lock.unlock();
        try {
            pollForTask();
        } catch (...) {
        }
        lock.lock();
    }
}

void WorkerClient::pollForTask() {
    const auto response = transport_(
        Message{MessageType::RequestTask, workerId_}.serialize());
    const auto parsed = Message::parse(response);
    if (!parsed.has_value()) {
        return;
    }

    if (parsed->type != MessageType::AssignTask) {
        return;
    }

    if (!parsed->jobId.has_value() || !parsed->taskId.has_value() || parsed->taskName.empty()) {
        return;
    }

    TaskHandler handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = taskHandlers_.find(parsed->taskName);
        if (it != taskHandlers_.end()) {
            handler = it->second;
        }
    }

    Message resultMessage{
        MessageType::TaskResult,
        workerId_,
        parsed->jobId,
        parsed->taskId
    };

    if (!handler) {
        resultMessage.success = false;
        resultMessage.errorMessage = "unknown_task_handler";
        static_cast<void>(transport_(resultMessage.serialize()));
        return;
    }

    scheduler::core::Task task{
        *parsed->taskId,
        parsed->taskName,
        std::move(handler),
        0,
        std::chrono::milliseconds(parsed->timeoutMs.value_or(0))
    };

    const auto result = taskExecutor_.execute(task);
    resultMessage.success = result.success;
    resultMessage.errorMessage = normalizeError(result.errorMessage);
    static_cast<void>(transport_(resultMessage.serialize()));
}

std::string WorkerClient::normalizeError(std::string message) {
    for (char& ch : message) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            ch = '_';
        }
    }

    return message;
}

}  // namespace scheduler::distributed
