
#pragma once

#include "Job.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(
        std::size_t threadCount,
        std::chrono::milliseconds agingInterval =
            std::chrono::milliseconds(1000)
    );

    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void enqueue(Job job);

private:
    void workerLoop();

    Job takeHighestPriorityJob();

    std::vector<std::thread> workers_;
    std::vector<Job> tasks_;

    std::mutex mutex_;
    std::condition_variable condition_;

    bool stopping_ = false;
    std::uint64_t nextJobId_ = 0;

    std::chrono::milliseconds agingInterval_;
};
