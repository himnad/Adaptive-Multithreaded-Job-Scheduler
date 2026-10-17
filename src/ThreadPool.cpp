
#include "ThreadPool.h"

#include <stdexcept>
#include <utility>

ThreadPool::ThreadPool(std::size_t threadCount) {
    if (threadCount == 0) {
        throw std::invalid_argument("Thread count must be positive");
    }

    workers_.reserve(threadCount);

    try {
        for (std::size_t i = 0; i < threadCount; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(mutex_);

                        condition_.wait(lock, [this] {
                            return stopping_ || !tasks_.empty();
                        });

                        if (stopping_ && tasks_.empty()) {
                            return;
                        }

                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }

                    try {
                        task();
                    } catch (...) {
                    }
                }
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

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (stopping_) {
            throw std::runtime_error("Cannot enqueue into stopped pool");
        }

        if (!task) {
            throw std::invalid_argument("Task cannot be empty");
        }

        tasks_.push(std::move(task));
    }

    condition_.notify_one();
}
