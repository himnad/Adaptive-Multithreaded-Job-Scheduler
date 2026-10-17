
#include "ThreadPool.h"

#include <chrono>
#include <future>
#include <iostream>

int main() {
    using namespace std::chrono_literals;

    ThreadPool pool(1, 1000ms);

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
        Priority::High,
        [] {
            std::cout << "High priority job executed\n";
        },
        {}
    });

    pool.enqueue(Job{
        0,
        Priority::Medium,
        [] {
            std::cout << "Urgent deadline job executed\n";
        },
        {},
        std::chrono::steady_clock::now() + 150ms
    });

    releaseBlocker.set_value();

    return 0;
}
