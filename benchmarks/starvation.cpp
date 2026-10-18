
#include "ThreadPool.h"

#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

void runTest(
    SchedulingPolicy policy,
    const char* policyName
) {
    ThreadPool pool(1, 1000ms, policy);

    std::promise<void> blockerStarted;
    std::promise<void> releaseBlocker;

    auto started = blockerStarted.get_future();
    auto release = releaseBlocker.get_future().share();

    pool.enqueue(Job{
        0,
        Priority::High,
        [&] {
            blockerStarted.set_value();
            release.wait();
        },
        {}
    });

    started.wait();

    pool.enqueue(Job{
        0,
        Priority::Low,
        [] {
            std::cout << "LOW job executed\n";
        },
        {}
    });

    std::this_thread::sleep_for(650ms);

    pool.enqueue(Job{
        0,
        Priority::High,
        [] {
            std::cout << "HIGH job executed\n";
        },
        {}
    });

    for (int i = 0; i < 8; ++i) {
        pool.enqueue(Job{
            0,
            Priority::Low,
            [] {},
            {}
        });
    }

    std::cout << "\n" << policyName << ":\n";

    releaseBlocker.set_value();
}

int main() {
    runTest(
        SchedulingPolicy::FixedPriority,
        "Fixed Priority"
    );

    runTest(
        SchedulingPolicy::AdaptivePriority,
        "Adaptive Priority"
    );

    return 0;
}
