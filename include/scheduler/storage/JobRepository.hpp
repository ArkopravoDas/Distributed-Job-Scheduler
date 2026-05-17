#pragma once

#include <optional>

#include "scheduler/core/Job.hpp"

namespace scheduler::storage {

class JobRepository {
public:
    virtual ~JobRepository() = default;

    virtual void saveJob(const scheduler::core::Job& job) = 0;
    virtual std::optional<scheduler::core::Job> getJob(scheduler::core::JobId jobId) = 0;
    virtual void updateJob(const scheduler::core::Job& job) = 0;
    virtual bool exists(scheduler::core::JobId jobId) const = 0;
};

}  // namespace scheduler::storage
