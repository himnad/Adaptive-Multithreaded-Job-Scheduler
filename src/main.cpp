
#include "ThreadPool.h"

#include <chrono>
#include <future>
#include <iostream>
#include <thread>

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
        Priority::Low,
        [] {
            std::cout << "OLD LOW priority job executed\n";
        },
        {}
    });

    std::this_thread::sleep_for(650ms);

    pool.enqueue(Job{
        0,
        Priority::High,
        [] {
            std::cout << "NEW HIGH priority job executed\n";
        },
        {}
    });

    for (int i = 0; i < 8; ++i) {
        pool.enqueue(Job{
            0,
            Priority::Low,
            [i] {
                std::cout << "Extra job " << i << " executed\n";
            },
            {}
        });
    }

    releaseBlocker.set_value();

    return 0;
}
