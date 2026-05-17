#pragma once

#include <mutex>
#include <optional>
#include <unordered_map>

#include "scheduler/storage/JobRepository.hpp"

namespace scheduler::storage {

class InMemoryJobRepository : public JobRepository {
public:
    void saveJob(const scheduler::core::Job& job) override;
    std::optional<scheduler::core::Job> getJob(scheduler::core::JobId jobId) override;
    void updateJob(const scheduler::core::Job& job) override;
    bool exists(scheduler::core::JobId jobId) const override;

private:
    mutable std::mutex mutex_;
    std::unordered_map<scheduler::core::JobId, scheduler::core::Job> jobs_;
};

}  // namespace scheduler::storage
