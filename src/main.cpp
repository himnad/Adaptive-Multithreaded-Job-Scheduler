
#include "ThreadPool.h"

#include <atomic>
#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <thread>

int main() {
    using namespace std::chrono_literals;

    ThreadPool pool(4, 1000ms);

    constexpr int totalJobs = 20;

    const auto benchmarkStart =
        std::chrono::steady_clock::now();

    std::promise<void> allJobsFinished;
    auto finished = allJobsFinished.get_future();

    std::atomic<int> remainingJobs{totalJobs};

    for (int i = 0; i < totalJobs; ++i) {
        Priority priority;

        if (i % 3 == 0) {
            priority = Priority::High;
        } else if (i % 3 == 1) {
            priority = Priority::Medium;
        } else {
            priority = Priority::Low;
        }

        pool.enqueue(Job{
            0,
            priority,
            [&remainingJobs, &allJobsFinished] {
                std::this_thread::sleep_for(100ms);

                if (remainingJobs.fetch_sub(1) == 1) {
                    allJobsFinished.set_value();
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

    const auto benchmarkEnd =
        std::chrono::steady_clock::now();

    const double elapsedSeconds =
        std::chrono::duration<double>(
            benchmarkEnd - benchmarkStart
        ).count();

    const double throughput =
        elapsedSeconds > 0.0
            ? metrics.completedJobs / elapsedSeconds
            : 0.0;

    std::cout << std::fixed << std::setprecision(2);

    std::cout << "\n===== SCHEDULER METRICS =====\n";

    std::cout << "Completed Jobs: "
              << metrics.completedJobs << '\n';

    std::cout << "Failed Jobs: "
              << metrics.failedJobs << '\n';

    std::cout << "Deadline Misses: "
              << metrics.deadlineMisses << '\n';

    std::cout << "Average Wait Time: "
              << metrics.averageWaitMs << " ms\n";

    std::cout << "Maximum Wait Time: "
              << metrics.maxWaitMs << " ms\n";

    std::cout << "Elapsed Time: "
              << elapsedSeconds << " seconds\n";

    std::cout << "Throughput: "
              << throughput << " jobs/sec\n";

    return 0;
}
