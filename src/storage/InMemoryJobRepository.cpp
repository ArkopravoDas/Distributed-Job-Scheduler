#include "scheduler/storage/InMemoryJobRepository.hpp"

#include <mutex>

namespace scheduler::storage {

void InMemoryJobRepository::saveJob(const scheduler::core::Job& job) {
    std::lock_guard<std::mutex> lock(mutex_);
    jobs_.insert_or_assign(job.id(), job);
}

std::optional<scheduler::core::Job> InMemoryJobRepository::getJob(scheduler::core::JobId jobId) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = jobs_.find(jobId);
    if (it == jobs_.end()) {
        return std::nullopt;
    }

    return it->second;
}

void InMemoryJobRepository::updateJob(const scheduler::core::Job& job) {
    std::lock_guard<std::mutex> lock(mutex_);
    jobs_.insert_or_assign(job.id(), job);
}

bool InMemoryJobRepository::exists(scheduler::core::JobId jobId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return jobs_.contains(jobId);
}

}  // namespace scheduler::storage
