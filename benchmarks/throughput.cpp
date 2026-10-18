
#include "ThreadPool.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std::chrono_literals;

struct BenchmarkResult {
    std::size_t workers;
    int jobs;
    double medianThroughput;
    double minThroughput;
    double maxThroughput;
};

double runOnce(std::size_t workers, int totalJobs) {
    std::atomic<int> remaining{totalJobs};
    std::atomic<std::uint64_t> checksum{0};

    std::promise<void> allDone;
    auto finished = allDone.get_future();

    ThreadPool pool(
        workers,
        1000ms,
        SchedulingPolicy::AdaptivePriority
    );

    const auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < totalJobs; ++i) {
        pool.enqueue(Job{
            0,
            Priority::Medium,
            [&, i] {
                std::uint64_t value =
                    static_cast<std::uint64_t>(i + 1);

                for (int j = 0; j < 2000; ++j) {
                    value = value * 1664525ULL + 1013904223ULL;
                    value ^= value >> 13;
                }

                checksum.fetch_xor(
                    value,
                    std::memory_order_relaxed
                );

                if (remaining.fetch_sub(
                        1, std::memory_order_acq_rel) == 1) {
                    allDone.set_value();
                }
            },
            {}
        });
    }

    finished.wait();

    const auto end = std::chrono::steady_clock::now();

    const double seconds =
        std::chrono::duration<double>(end - start).count();

    if (remaining.load() != 0 || seconds <= 0.0) {
        throw std::runtime_error("Benchmark validation failed");
    }

    const auto observedChecksum = checksum.load();
    (void)observedChecksum;

    return static_cast<double>(totalJobs) / seconds;
}

BenchmarkResult benchmark(std::size_t workers, int jobs) {
    constexpr int repetitions = 5;

    std::vector<double> results;
    results.reserve(repetitions);

    for (int run = 0; run < repetitions; ++run) {
        results.push_back(runOnce(workers, jobs));
    }

    std::sort(results.begin(), results.end());

    return {
        workers,
        jobs,
        results[repetitions / 2],
        results.front(),
        results.back()
    };
}

int main() {
    std::ofstream csv("throughput_results.csv");

    if (!csv) {
        std::cerr << "Unable to create CSV file\n";
        return 1;
    }

    csv << "Workers,Jobs,MedianJobsPerSec,"
           "MinJobsPerSec,MaxJobsPerSec\n";

    std::cout << std::fixed << std::setprecision(2);
    csv << std::fixed << std::setprecision(2);

    std::cout << "\n===== RELIABLE THROUGHPUT BENCHMARK =====\n";
    std::cout << "Release build recommended | 5 runs per configuration\n\n";

    std::cout << std::left
              << std::setw(10) << "Workers"
              << std::setw(10) << "Jobs"
              << std::setw(20) << "Median jobs/sec"
              << std::setw(20) << "Min jobs/sec"
              << std::setw(20) << "Max jobs/sec"
              << '\n';

    for (int jobs : {10000, 20000}) {
        for (std::size_t workers : {1, 2, 4, 8}) {
            const auto result = benchmark(workers, jobs);

            std::cout << std::left
                      << std::setw(10) << result.workers
                      << std::setw(10) << result.jobs
                      << std::setw(20) << result.medianThroughput
                      << std::setw(20) << result.minThroughput
                      << std::setw(20) << result.maxThroughput
                      << '\n';

            csv << result.workers << ','
                << result.jobs << ','
                << result.medianThroughput << ','
                << result.minThroughput << ','
                << result.maxThroughput << '\n';
        }
    }

    std::cout << "\nResults saved to throughput_results.csv\n";
    return 0;
}
