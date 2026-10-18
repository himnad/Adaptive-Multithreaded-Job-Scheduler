
#include <gtest/gtest.h>

#include "ThreadPool.h"

#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;

TEST(ThreadPoolTest, RejectsZeroWorkers) {
    EXPECT_THROW(ThreadPool(0), std::invalid_argument);
}

TEST(ThreadPoolTest, RejectsZeroAgingInterval) {
    EXPECT_THROW(ThreadPool(2, 0ms), std::invalid_argument);
}

TEST(ThreadPoolTest, RejectsNegativeAgingInterval) {
    EXPECT_THROW(ThreadPool(2, -100ms), std::invalid_argument);
}

TEST(ThreadPoolTest, ExecutesSubmittedJob) {
    ThreadPool pool(2);

    std::promise<int> result;
    auto future = result.get_future();

    pool.enqueue(Job{
        0,
        Priority::Medium,
        [&result] {
            result.set_value(42);
        },
        {}
    });

    ASSERT_EQ(future.wait_for(3s), std::future_status::ready);
    EXPECT_EQ(future.get(), 42);
}

TEST(ThreadPoolTest, RejectsEmptyTask) {
    ThreadPool pool(1);

    EXPECT_THROW(
        pool.enqueue(Job{
            0,
            Priority::Low,
            {},
            {}
        }),
        std::invalid_argument
    );
}

TEST(ThreadPoolTest, ExecutesMultipleJobs) {
    std::atomic<int> completed{0};

    {
        ThreadPool pool(4);

        for (int i = 0; i < 20; ++i) {
            pool.enqueue(Job{
                0,
                Priority::Medium,
                [&completed] {
                    completed.fetch_add(1);
                },
                {}
            });
        }
    }

    EXPECT_EQ(completed.load(), 20);
}

TEST(ThreadPoolTest, RunsJobsConcurrently) {
    std::atomic<int> active{0};
    std::atomic<int> maxActive{0};

    {
        ThreadPool pool(4);

        for (int i = 0; i < 4; ++i) {
            pool.enqueue(Job{
                0,
                Priority::Medium,
                [&] {
                    int current = ++active;
                    int previous = maxActive.load();

                    while (current > previous &&
                           !maxActive.compare_exchange_weak(previous, current)) {
                    }

                    std::this_thread::sleep_for(100ms);
                    --active;
                },
                {}
            });
        }
    }

    EXPECT_GT(maxActive.load(), 1);
    EXPECT_EQ(active.load(), 0);
}

TEST(ThreadPoolTest, CountsCompletedJobs) {
    ThreadPool pool(2);

    std::promise<void> allDone;
    auto doneFuture = allDone.get_future();
    std::atomic<int> completed{0};

    for (int i = 0; i < 10; ++i) {
        pool.enqueue(Job{
            0,
            Priority::Low,
            [&] {
                if (completed.fetch_add(1) + 1 == 10) {
                    allDone.set_value();
                }
            },
            {}
        });
    }

    ASSERT_EQ(
        doneFuture.wait_for(3s),
        std::future_status::ready
    );

    for (int i = 0; i < 100; ++i) {
        if (pool.getMetrics().completedJobs == 10) {
            break;
        }

        std::this_thread::sleep_for(1ms);
    }

    EXPECT_EQ(pool.getMetrics().completedJobs, 10);
}

TEST(ThreadPoolTest, HandlesJobExceptions) {
    ThreadPool pool(2);

    pool.enqueue(Job{
        0,
        Priority::High,
        [] {
            throw std::runtime_error("Test exception");
        },
        {}
    });

    for (int i = 0; i < 100; ++i) {
        if (pool.getMetrics().failedJobs == 1) {
            break;
        }

        std::this_thread::sleep_for(1ms);
    }

    EXPECT_EQ(pool.getMetrics().failedJobs, 1);
}

TEST(ThreadPoolTest, GracefulShutdownCompletesPendingJobs) {
    std::atomic<int> completed{0};

    {
        ThreadPool pool(2);

        for (int i = 0; i < 15; ++i) {
            pool.enqueue(Job{
                0,
                Priority::Medium,
                [&completed] {
                    std::this_thread::sleep_for(5ms);
                    completed.fetch_add(1);
                },
                {}
            });
        }
    }

    EXPECT_EQ(completed.load(), 15);
}
