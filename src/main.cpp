
#include "ThreadPool.h"

#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

int main() {
    using namespace std::chrono_literals;

    std::mutex outputMutex;

    ThreadPool pool(1, 200ms);

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
        [&] {
            std::lock_guard<std::mutex> lock(outputMutex);
            std::cout << "Low priority job executed\n";
        },
        {}
    });

    std::this_thread::sleep_for(650ms);

    pool.enqueue(Job{
        0,
        Priority::High,
        [&] {
            std::lock_guard<std::mutex> lock(outputMutex);
            std::cout << "High priority job executed\n";
        },
        {}
    });

    releaseBlocker.set_value();

    return 0;
}
