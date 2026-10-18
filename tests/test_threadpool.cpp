
#include <gtest/gtest.h>

#include "ThreadPool.h"

#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>
#include <vector>

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

TEST(ThreadPoolTest, FIFOExecutesJobsInSubmissionOrder) {
    std::vector<int> executionOrder;

    std::promise<void> blockerStarted;
    std::promise<void> releaseBlocker;

    auto started = blockerStarted.get_future();
    auto release = releaseBlocker.get_future().share();

    {
        ThreadPool pool(1, 1000ms, SchedulingPolicy::FIFO);

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

        for (int i = 1; i <= 5; ++i) {
            pool.enqueue(Job{
                0,
                Priority::Medium,
                [&, i] {
                    executionOrder.push_back(i);
                },
                {}
            });
        }

        releaseBlocker.set_value();
    }

    EXPECT_EQ(
        executionOrder,
        (std::vector<int>{1, 2, 3, 4, 5})
    );
}

TEST(ThreadPoolTest, FixedPriorityExecutesHighFirst) {
    std::vector<int> executionOrder;

    std::promise<void> blockerStarted;
    std::promise<void> releaseBlocker;

    auto started = blockerStarted.get_future();
    auto release = releaseBlocker.get_future().share();

    {
        ThreadPool pool(1, 1000ms, SchedulingPolicy::FixedPriority);

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
            [&] { executionOrder.push_back(1); },
            {}
        });

        pool.enqueue(Job{
            0,
            Priority::Medium,
            [&] { executionOrder.push_back(2); },
            {}
        });

        pool.enqueue(Job{
            0,
            Priority::High,
            [&] { executionOrder.push_back(3); },
            {}
        });

        releaseBlocker.set_value();
    }

    EXPECT_EQ(
        executionOrder,
        (std::vector<int>{3, 2, 1})
    );
}

TEST(ThreadPoolTest, AdaptiveAgingPreventsLowPriorityStarvation) {
    std::vector<int> executionOrder;

    std::promise<void> blockerStarted;
    std::promise<void> releaseBlocker;

    auto started = blockerStarted.get_future();
    auto release = releaseBlocker.get_future().share();

    {
        ThreadPool pool(1, 1000ms, SchedulingPolicy::AdaptivePriority);

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
                executionOrder.push_back(1);
            },
            {}
        });

        std::this_thread::sleep_for(650ms);

        pool.enqueue(Job{
            0,
            Priority::High,
            [&] {
                executionOrder.push_back(2);
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

        releaseBlocker.set_value();
    }

    ASSERT_GE(executionOrder.size(), 2u);
    EXPECT_EQ(executionOrder[0], 1);
    EXPECT_EQ(executionOrder[1], 2);
}

TEST(ThreadPoolTest, OverdueDeadlineGetsHigherPriority) {
    std::vector<int> executionOrder;

    std::promise<void> blockerStarted;
    std::promise<void> releaseBlocker;

    auto started = blockerStarted.get_future();
    auto release = releaseBlocker.get_future().share();

    {
        ThreadPool pool(1, 1000ms, SchedulingPolicy::AdaptivePriority);

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

        Job overdueJob{
            0,
            Priority::Low,
            [&] {
                executionOrder.push_back(1);
            },
            {}
        };

        overdueJob.deadline =
            std::chrono::steady_clock::now() - 1s;

        pool.enqueue(std::move(overdueJob));

        pool.enqueue(Job{
            0,
            Priority::High,
            [&] {
                executionOrder.push_back(2);
            },
            {}
        });

        releaseBlocker.set_value();
    }

    EXPECT_EQ(
        executionOrder,
        (std::vector<int>{1, 2})
    );
}

TEST(ThreadPoolTest, CountsMissedDeadlines) {
    ThreadPool pool(1);

    std::promise<void> jobStarted;
    auto started = jobStarted.get_future();

    Job job{
        0,
        Priority::Medium,
        [&] {
            jobStarted.set_value();
        },
        {}
    };

    job.deadline = std::chrono::steady_clock::now() - 1s;

    pool.enqueue(std::move(job));

    ASSERT_EQ(
        started.wait_for(3s),
        std::future_status::ready
    );

    for (int i = 0; i < 100; ++i) {
        if (pool.getMetrics().deadlineMisses == 1) {
            break;
        }

        std::this_thread::sleep_for(1ms);
    }

    EXPECT_EQ(pool.getMetrics().deadlineMisses, 1);
}

TEST(ThreadPoolTest, ContinuesExecutingAfterJobFailure) {
    std::atomic<int> completed{0};

    {
        ThreadPool pool(1);

        pool.enqueue(Job{
            0,
            Priority::High,
            [] {
                throw std::runtime_error("Intentional failure");
            },
            {}
        });

        pool.enqueue(Job{
            0,
            Priority::Medium,
            [&] {
                completed.fetch_add(1);
            },
            {}
        });
    }

    EXPECT_EQ(completed.load(), 1);
}

TEST(ThreadPoolTest, FIFOIgnoresJobPriority) {
    std::vector<int> executionOrder;

    std::promise<void> blockerStarted;
    std::promise<void> releaseBlocker;

    auto started = blockerStarted.get_future();
    auto release = releaseBlocker.get_future().share();

    {
        ThreadPool pool(1, 1000ms, SchedulingPolicy::FIFO);

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
                executionOrder.push_back(1);
            },
            {}
        });

        pool.enqueue(Job{
            0,
            Priority::High,
            [&] {
                executionOrder.push_back(2);
            },
            {}
        });

        pool.enqueue(Job{
            0,
            Priority::Medium,
            [&] {
                executionOrder.push_back(3);
            },
            {}
        });

        releaseBlocker.set_value();
    }

    EXPECT_EQ(
        executionOrder,
        (std::vector<int>{1, 2, 3})
    );
}

TEST(ThreadPoolTest, SingleWorkerNeverRunsJobsConcurrently) {
    std::atomic<int> active{0};
    std::atomic<int> maxActive{0};

    {
        ThreadPool pool(1);

        for (int i = 0; i < 20; ++i) {
            pool.enqueue(Job{
                0,
                Priority::Medium,
                [&] {
                    int current = ++active;
                    int previous = maxActive.load();

                    while (
                        current > previous &&
                        !maxActive.compare_exchange_weak(previous, current)
                    ) {
                    }

                    std::this_thread::sleep_for(1ms);
                    --active;
                },
                {}
            });
        }
    }

    EXPECT_EQ(maxActive.load(), 1);
    EXPECT_EQ(active.load(), 0);
}

TEST(ThreadPoolTest, MultipleProducersSubmitJobsSafely) {
    std::atomic<int> completed{0};

    {
        ThreadPool pool(4);
        std::vector<std::thread> producers;

        for (int p = 0; p < 4; ++p) {
            producers.emplace_back([&] {
                for (int i = 0; i < 50; ++i) {
                    pool.enqueue(Job{
                        0,
                        Priority::Medium,
                        [&] {
                            completed.fetch_add(1);
                        },
                        {}
                    });
                }
            });
        }

        for (auto& producer : producers) {
            producer.join();
        }
    }

    EXPECT_EQ(completed.load(), 200);
}

TEST(ThreadPoolTest, FixedPriorityPreservesFIFOForEqualPriorities) {
    std::vector<int> order;
    std::promise<void> startedPromise;
    std::promise<void> releasePromise;

    auto started = startedPromise.get_future();
    auto release = releasePromise.get_future().share();

    {
        ThreadPool pool(1, 1000ms, SchedulingPolicy::FixedPriority);

        pool.enqueue(Job{
            0, Priority::High,
            [&] {
                startedPromise.set_value();
                release.wait();
            }, {}
        });

        started.wait();

        for (int i = 1; i <= 5; ++i) {
            pool.enqueue(Job{
                0, Priority::High,
                [&, i] { order.push_back(i); }, {}
            });
        }

        releasePromise.set_value();
    }

    EXPECT_EQ(order, (std::vector<int>{1, 2, 3, 4, 5}));
}

TEST(ThreadPoolTest, ZeroJobsProducesZeroMetrics) {
    ThreadPool pool(2);
    auto metrics = pool.getMetrics();

    EXPECT_EQ(metrics.completedJobs, 0);
    EXPECT_EQ(metrics.failedJobs, 0);
    EXPECT_EQ(metrics.deadlineMisses, 0);
    EXPECT_DOUBLE_EQ(metrics.averageWaitMs, 0.0);
    EXPECT_DOUBLE_EQ(metrics.maxWaitMs, 0.0);
}

TEST(ThreadPoolTest, SuccessfulJobsDoNotIncreaseFailureCount) {
    ThreadPool pool(2);
    std::promise<void> donePromise;
    auto done = donePromise.get_future();

    pool.enqueue(Job{
        0, Priority::Medium,
        [&] { donePromise.set_value(); }, {}
    });

    ASSERT_EQ(done.wait_for(3s), std::future_status::ready);

    EXPECT_EQ(pool.getMetrics().failedJobs, 0);
}

TEST(ThreadPoolTest, FailedJobDoesNotCountAsCompleted) {
    ThreadPool pool(1);

    pool.enqueue(Job{
        0, Priority::High,
        [] { throw std::runtime_error("failure"); }, {}
    });

    for (int i = 0; i < 100; ++i) {
        if (pool.getMetrics().failedJobs == 1) {
            break;
        }
        std::this_thread::sleep_for(1ms);
    }

    auto metrics = pool.getMetrics();

    EXPECT_EQ(metrics.failedJobs, 1);
    EXPECT_EQ(metrics.completedJobs, 0);
}

TEST(ThreadPoolTest, JobWithoutDeadlineDoesNotMissDeadline) {
    ThreadPool pool(1);
    std::promise<void> donePromise;
    auto done = donePromise.get_future();

    pool.enqueue(Job{
        0, Priority::Medium,
        [&] { donePromise.set_value(); }, {}
    });

    ASSERT_EQ(done.wait_for(3s), std::future_status::ready);

    EXPECT_EQ(pool.getMetrics().deadlineMisses, 0);
}

TEST(ThreadPoolTest, WaitMetricsAreNonNegative) {
    ThreadPool pool(2);
    std::promise<void> donePromise;
    auto done = donePromise.get_future();

    pool.enqueue(Job{
        0, Priority::Medium,
        [&] { donePromise.set_value(); }, {}
    });

    ASSERT_EQ(done.wait_for(3s), std::future_status::ready);

    auto metrics = pool.getMetrics();

    EXPECT_GE(metrics.averageWaitMs, 0.0);
    EXPECT_GE(metrics.maxWaitMs, 0.0);
}





