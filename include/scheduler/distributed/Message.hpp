#pragma once

#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include "scheduler/core/SchedulerTypes.hpp"

namespace scheduler::distributed {

enum class MessageType {
    RegisterWorker,
    Heartbeat,
    RequestTask,
    AssignTask,
    TaskResult,
    Ack,
    Error,
    Unknown
};

struct Message {
    MessageType type{MessageType::Unknown};
    std::string workerId;
    std::optional<scheduler::core::JobId> jobId;
    std::optional<scheduler::core::TaskId> taskId;
    std::string taskName;
    std::optional<bool> success;
    std::optional<std::uint64_t> timeoutMs;
    std::string errorMessage;
    std::string body;

    std::string serialize() const {
        std::ostringstream stream;
        stream << toString(type);
        if (!workerId.empty()) {
            stream << " worker_id=" << workerId;
        }
        if (jobId.has_value()) {
            stream << " job_id=" << *jobId;
        }
        if (taskId.has_value()) {
            stream << " task_id=" << *taskId;
        }
        if (!taskName.empty()) {
            stream << " task_name=" << taskName;
        }
        if (success.has_value()) {
            stream << " success=" << (*success ? "1" : "0");
        }
        if (timeoutMs.has_value()) {
            stream << " timeout_ms=" << *timeoutMs;
        }
        if (!errorMessage.empty()) {
            stream << " error=" << errorMessage;
        }
        if (!body.empty()) {
            stream << " body=" << body;
        }

        return stream.str();
    }

    static std::optional<Message> parse(std::string_view raw) {
        std::istringstream stream{std::string(raw)};
        std::string typeToken;
        stream >> typeToken;
        if (typeToken.empty()) {
            return std::nullopt;
        }

        Message message;
        message.type = fromString(typeToken);

        std::string token;
        while (stream >> token) {
            const auto separator = token.find('=');
            if (separator == std::string::npos) {
                continue;
            }

            const auto key = token.substr(0, separator);
            const auto value = token.substr(separator + 1);

            if (key == "worker_id") {
                message.workerId = value;
            } else if (key == "job_id") {
                message.jobId = static_cast<scheduler::core::JobId>(std::stoull(value));
            } else if (key == "task_id") {
                message.taskId = static_cast<scheduler::core::TaskId>(std::stoull(value));
            } else if (key == "task_name") {
                message.taskName = value;
            } else if (key == "success") {
                message.success = value == "1" || value == "true";
            } else if (key == "timeout_ms") {
                message.timeoutMs = std::stoull(value);
            } else if (key == "error") {
                message.errorMessage = value;
            } else if (key == "body") {
                message.body = value;
            }
        }

        return message;
    }

    static std::string toString(MessageType type) {
        switch (type) {
            case MessageType::RegisterWorker:
                return "REGISTER";
            case MessageType::Heartbeat:
                return "HEARTBEAT";
            case MessageType::RequestTask:
                return "REQUEST_TASK";
            case MessageType::AssignTask:
                return "ASSIGN_TASK";
            case MessageType::TaskResult:
                return "TASK_RESULT";
            case MessageType::Ack:
                return "ACK";
            case MessageType::Error:
                return "ERROR";
            case MessageType::Unknown:
                return "UNKNOWN";
        }

        return "UNKNOWN";
    }

    static MessageType fromString(std::string_view rawType) {
        if (rawType == "REGISTER") {
            return MessageType::RegisterWorker;
        }
        if (rawType == "HEARTBEAT") {
            return MessageType::Heartbeat;
        }
        if (rawType == "REQUEST_TASK") {
            return MessageType::RequestTask;
        }
        if (rawType == "ASSIGN_TASK") {
            return MessageType::AssignTask;
        }
        if (rawType == "TASK_RESULT") {
            return MessageType::TaskResult;
        }
        if (rawType == "ACK") {
            return MessageType::Ack;
        }
        if (rawType == "ERROR") {
            return MessageType::Error;
        }

        return MessageType::Unknown;
    }
};

}  // namespace scheduler::distributed
