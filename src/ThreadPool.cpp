
#include "ThreadPool.h"

#include <chrono>
#include <stdexcept>
#include <utility>

ThreadPool::ThreadPool(
    std::size_t threadCount,
    std::chrono::milliseconds agingInterval
) : agingInterval_(agingInterval) {

    if (threadCount == 0) {
        throw std::invalid_argument("Thread count must be positive");
    }

    if (agingInterval_.count() <= 0) {
        throw std::invalid_argument("Aging interval must be positive");
    }

    workers_.reserve(threadCount);

    try {
        for (std::size_t i = 0; i < threadCount; ++i) {
            workers_.emplace_back([this] {
                workerLoop();
            });
        }
    } catch (...) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }

        condition_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        throw;
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }

    condition_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::enqueue(Job job) {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (stopping_) {
            throw std::runtime_error("Cannot enqueue into stopped pool");
        }

        if (!job.task) {
            throw std::invalid_argument("Task cannot be empty");
        }

        job.id = nextJobId_;
        job.submittedAt = std::chrono::steady_clock::now();

        tasks_.push_back(std::move(job));
        ++nextJobId_;
    }

    condition_.notify_one();
}

std::chrono::milliseconds
ThreadPool::getAdaptiveAgingInterval() const {
    const std::size_t queueSize = tasks_.size();

    if (queueSize >= 10) {
        return std::chrono::milliseconds(200);
    }

    if (queueSize >= 5) {
        return std::chrono::milliseconds(500);
    }

    return agingInterval_;
}

Job ThreadPool::takeHighestPriorityJob() {
    const auto now = std::chrono::steady_clock::now();
    const auto adaptiveInterval = getAdaptiveAgingInterval();

    auto calculateScore = [&](const Job& job) {
        const auto waitingTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - job.submittedAt
            ).count();

        std::int64_t score =
            static_cast<std::int64_t>(job.priority) +
            waitingTime / adaptiveInterval.count();

        if (job.deadline.has_value()) {
            const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    job.deadline.value() - now
                ).count();

            if (remaining <= 0) {
                score += 10;
            } else if (remaining <= 200) {
                score += 5;
            } else if (remaining <= 1000) {
                score += 2;
            }
        }

        return score;
    };

    auto best = tasks_.begin();

    for (auto it = tasks_.begin() + 1; it != tasks_.end(); ++it) {
        const auto currentScore = calculateScore(*it);
        const auto bestScore = calculateScore(*best);

        if (currentScore > bestScore ||
            (currentScore == bestScore && it->id < best->id)) {
            best = it;
        }
    }

    Job selected = std::move(*best);
    tasks_.erase(best);

    return selected;
}

void ThreadPool::workerLoop() {
    while (true) {
        Job job;

        {
            std::unique_lock<std::mutex> lock(mutex_);

            condition_.wait(lock, [this] {
                return stopping_ || !tasks_.empty();
            });

            if (stopping_ && tasks_.empty()) {
                return;
            }

            job = takeHighestPriorityJob();
        }

        try {
            job.task();
        } catch (...) {
        }
    }
}
