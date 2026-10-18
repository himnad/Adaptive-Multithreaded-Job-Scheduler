
#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>

struct MetricsSnapshot {
    std::uint64_t completedJobs = 0;
    std::uint64_t failedJobs = 0;
    std::uint64_t deadlineMisses = 0;
    double averageWaitMs = 0.0;
    double maxWaitMs = 0.0;
};

class SchedulerMetrics {
public:
    void recordJob(
        std::chrono::steady_clock::duration waitingTime,
        bool missedDeadline,
        bool succeeded
    ) {
        const double waitMs =
            std::chrono::duration<double, std::milli>(
                waitingTime
            ).count();

        std::lock_guard<std::mutex> lock(mutex_);

        if (succeeded) {
            ++completedJobs_;
        } else {
            ++failedJobs_;
        }

        if (missedDeadline) {
            ++deadlineMisses_;
        }

        totalWaitMs_ += waitMs;
        maxWaitMs_ = std::max(maxWaitMs_, waitMs);
    }

    MetricsSnapshot snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);

        const auto totalJobs = completedJobs_ + failedJobs_;

        return {
            completedJobs_,
            failedJobs_,
            deadlineMisses_,
            totalJobs == 0
                ? 0.0
                : totalWaitMs_ / static_cast<double>(totalJobs),
            maxWaitMs_
        };
    }

private:
    mutable std::mutex mutex_;

    std::uint64_t completedJobs_ = 0;
    std::uint64_t failedJobs_ = 0;
    std::uint64_t deadlineMisses_ = 0;

    double totalWaitMs_ = 0.0;
    double maxWaitMs_ = 0.0;
};
