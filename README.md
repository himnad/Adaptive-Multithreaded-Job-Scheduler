# Adaptive Multithreaded Job Scheduler

A job scheduler written in **C++20**. It runs many tasks at the same time using multiple threads, and it decides **which task should run next** based on priority, waiting time, and deadlines.

The big question behind this project: *when many tasks are waiting, which one should go first?*

Instead of running everything in the same order, this scheduler offers three different strategies so you can compare how they behave.

## Why I Built This

In real applications, many tasks arrive together. A server may handle user requests, background work, and urgent jobs all at once. But the number of threads is limited, so we cannot run everything immediately.

This causes some problems:

- Some tasks are more important than others.
- Low-priority tasks may wait too long.
- Some tasks have deadlines.
- Many threads must safely share the same queue.

I built this project to learn how **multithreading, synchronization, priority scheduling, and task management** work together in a real system.

## How It Works

The scheduler has two main parts: a shared queue of waiting jobs and a pool of worker threads.

1. A new job is submitted and added to the queue.
2. When a worker is free, the scheduler picks a job using the chosen policy.
3. The worker runs the job and then becomes free again for the next one.

```text
                 New Jobs
                    |
                    v
              Job Queue
                    |
                    v
            Scheduling Policy
                    |
          +---------+---------+
          |         |         |
         FIFO     Fixed    Adaptive
                 Priority   Priority
                    |
                    v
              Thread Pool
                    |
           +--------+--------+
           |        |        |
        Worker 1 Worker 2 Worker 3 ...
           |        |        |
           +--------+--------+
                    |
                    v
              Job Execution
                    |
                    v
            Performance Metrics
```

The queue is protected with a **mutex**, so two threads cannot change it at the same time. **Condition variables** let idle workers sleep until a new job arrives, instead of checking the queue again and again.

## Scheduling Strategies

### 1. FIFO (First In, First Out)

Jobs run in the same order they arrive. If Job A comes before Job B, Job A runs first.

It is simple, but it ignores how important a job is.

### 2. Fixed Priority

Every job is High, Medium, or Low priority. Higher-priority jobs always run first.

The problem: if high-priority jobs keep coming, low-priority jobs may wait forever. This is called **starvation**.

### 3. Adaptive Priority (main feature)

This policy looks at more than the original priority. It also checks **how long a job has been waiting**.

- **Priority aging:** the longer a job waits, the higher its effective priority becomes.
- **Smart aging speed:** the aging interval changes based on how many jobs are waiting.
- **Deadline boost:** jobs close to their deadline get extra priority.

This way, old jobs and urgent jobs do not get ignored.

## Key Features

- **Multithreaded execution:** many worker threads run jobs at the same time.
- **Thread-safe queue:** mutexes protect shared data.
- **Three scheduling policies:** FIFO, Fixed Priority, and Adaptive Priority.
- **Priority aging:** waiting jobs slowly gain priority.
- **Deadline awareness:** jobs near or past their deadline get extra priority.
- **Graceful shutdown:** pending jobs finish before the threads are joined.
- **Exception handling:** if one job fails, the rest of the pool keeps working.
- **Performance metrics:** tracks completed jobs, failed jobs, missed deadlines, and waiting times.

## Technologies Used

| Technology | Purpose |
|---|---|
| C++20 | Main language |
| STL | Containers, functions, and utilities |
| `std::thread` | Running worker threads |
| `std::mutex` | Protecting shared data |
| `std::condition_variable` | Letting workers wait and wake up |
| `std::atomic` | Thread-safe counters in benchmarks and tests |
| CMake | Building the project |
| GoogleTest | Unit testing |

## Project Structure

```text
Adaptive-Multithreaded-Job-Scheduler/
|
|-- include/
|   |-- Job.h
|   |-- ThreadPool.h
|   |-- SchedulerMetrics.h
|
|-- src/
|   |-- main.cpp
|   |-- ThreadPool.cpp
|
|-- benchmarks/
|   |-- compare.cpp
|   |-- starvation.cpp
|   |-- throughput.cpp
|
|-- tests/
|   |-- test_threadpool.cpp
|
|-- CMakeLists.txt
|-- throughput_results.csv
|-- README.md
```

## How to Run

You need a C++20 compiler and CMake.

**1. Build the project**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**2. Run the scheduler (Windows)**

```cmd
build\scheduler.exe
```

**3. Compare the scheduling strategies**

```cmd
build\scheduler_compare.exe
```

**4. See the starvation demo**

```cmd
build\scheduler_starvation.exe
```

**5. Measure throughput**

```cmd
build\scheduler_throughput.exe
```

## Performance Results

I tested different numbers of worker threads and jobs using a CPU-heavy synthetic workload. Each setup was run five times, and I recorded the median throughput.

| Worker Threads | 10,000 Jobs | 20,000 Jobs |
|---|---:|---:|
| 1 | 20,581 jobs/sec | 4,057 jobs/sec |
| 2 | 13,707 jobs/sec | 5,594 jobs/sec |
| 4 | 30,656 jobs/sec | 29,560 jobs/sec |
| 8 | 59,360 jobs/sec | 69,202 jobs/sec |

**Best result: about 69,200 jobs per second** (8 worker threads, 20,000 jobs).

More threads do not always mean better speed. The result depends on the workload, the hardware, and scheduling overhead.

Full results are in `throughput_results.csv`.

## Testing

I used GoogleTest to check that the scheduler works correctly. The tests cover:

- Running jobs concurrently and thread safety
- FIFO and priority ordering
- Adaptive priority aging
- Deadline handling
- Job failures and exceptions
- Graceful shutdown
- Multiple threads submitting jobs at the same time
- Performance metrics

Run all tests with:

```bash
ctest --test-dir build --output-on-failure
```

**Result: all 25 automated tests passed in the Release build.**

## What I Learned

- How thread pools manage many tasks at once
- Why mutexes and condition variables are important
- How different scheduling policies change job execution
- How priority aging solves starvation
- Why benchmarking multithreaded programs is tricky
- How to test concurrent code with GoogleTest

It also gave me hands-on practice with modern C++, CMake, and building a system that combines concurrency with scheduling.

## Future Improvements

- Make job selection faster for very large queues
- Automatically change the number of worker threads based on load
- Add more latency and fairness benchmarks
- Try more scheduling strategies

---

**Built with C++20 to explore multithreading, scheduling algorithms, and concurrent system design.**
