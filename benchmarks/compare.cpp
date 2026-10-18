
#include "ThreadPool.h"

#include <atomic>
#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

struct BenchmarkResult {
    MetricsSnapshot metrics;
    double elapsedSeconds;
    double throughput;
};

BenchmarkResult runBenchmark(SchedulingPolicy policy) {
    constexpr int totalJobs = 60;

    ThreadPool pool(1, 1000ms);

    std::atomic<int> remainingJobs{totalJobs};
    std::promise<void> allFinished;
    auto finished = allFinished.get_future();

    const auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < totalJobs; ++i) {
        Priority priority;

        if (i % 3 == 0) {
            priority = Priority::Low;
        } else if (i % 3 == 1) {
            priority = Priority::Medium;
        } else {
            priority = Priority::High;
        }

        pool.enqueue(Job{
            0,
            priority,
            [&remainingJobs, &allFinished] {
                std::this_thread::sleep_for(10ms);

                if (remainingJobs.fetch_sub(1) == 1) {
                    allFinished.set_value();
                }
            },
            {}
        });
    }

    finished.wait();

    MetricsSnapshot metrics;

    while (true) {
        metrics = pool.getMetrics();

        if (metrics.completedJobs + metrics.failedJobs ==
            totalJobs) {
            break;
        }

        std::this_thread::yield();
    }

    const auto end = std::chrono::steady_clock::now();

    const double elapsed =
        std::chrono::duration<double>(end - start).count();

    return {
        metrics,
        elapsed,
        metrics.completedJobs / elapsed
    };
}

int main() {
    std::cout << std::fixed << std::setprecision(2);

    const auto fifo =
        runBenchmark(SchedulingPolicy::FIFO);

    const auto fixed =
        runBenchmark(SchedulingPolicy::FixedPriority);

    const auto adaptive =
        runBenchmark(SchedulingPolicy::AdaptivePriority);

    std::cout << "\n===== SCHEDULER COMPARISON =====\n\n";

    std::cout << std::left << std::setw(20) << "Policy"
              << std::setw(18) << "Throughput"
              << std::setw(18) << "Avg Wait (ms)"
              << std::setw(18) << "Max Wait (ms)"
              << '\n';

    auto print = [](const char* name,
                    const BenchmarkResult& result) {
        std::cout << std::left << std::setw(20) << name
                  << std::setw(18) << result.throughput
                  << std::setw(18)
                  << result.metrics.averageWaitMs
                  << std::setw(18)
                  << result.metrics.maxWaitMs
                  << '\n';
    };

    print("FIFO", fifo);
    print("Fixed Priority", fixed);
    print("Adaptive Priority", adaptive);

    return 0;
}
