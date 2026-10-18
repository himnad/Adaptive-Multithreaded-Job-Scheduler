\# Adaptive Multithreaded Job Scheduler



A multithreaded job scheduling system built using \*\*C++20\*\* that executes multiple tasks efficiently while managing their priorities, waiting times, and deadlines.



The main idea behind this project is simple: \*\*when many tasks are waiting to execute, how do we decide which task should run next?\*\*



Instead of executing every task in the same order, this scheduler supports different scheduling strategies and allows us to compare their behavior.



\## Why I Built This Project



In real-world applications, multiple tasks often need to be processed at the same time. For example, a server might need to handle user requests, background processing, and important operations simultaneously.



Running every task immediately is not always possible because the number of available threads is limited.



This creates a few challenges:



\- Some tasks are more important than others.

\- Low-priority tasks may wait too long.

\- Certain tasks have deadlines.

\- Multiple threads must safely access the same task queue.



I built this project to understand how \*\*multithreading, synchronization, priority scheduling, and task management\*\* work together in a practical system.



\## How It Works



The scheduler maintains a shared queue of pending jobs and a pool of worker threads.



When a new job is submitted, it is added to the queue. Whenever a worker becomes available, the scheduler selects a job according to the configured scheduling policy.



The worker executes that job and then becomes available for another task.



```text

&#x20;                New Jobs

&#x20;                   |

&#x20;                   v

&#x20;             Job Queue

&#x20;                   |

&#x20;                   v

&#x20;           Scheduling Policy

&#x20;                   |

&#x20;         +---------+---------+

&#x20;         |         |         |

&#x20;        FIFO     Fixed    Adaptive

&#x20;               Priority   Priority

&#x20;                   |

&#x20;                   v

&#x20;             Thread Pool

&#x20;                   |

&#x20;          +--------+--------+

&#x20;          |        |        |

&#x20;       Worker 1 Worker 2 Worker 3 ...

&#x20;          |        |        |

&#x20;          +--------+--------+

&#x20;                   |

&#x20;                   v

&#x20;             Job Execution

&#x20;                   |

&#x20;                   v

&#x20;            Performance Metrics

```



The shared queue is protected using a mutex, while condition variables allow idle workers to wait for new jobs without continuously checking the queue.



\## Scheduling Strategies



The project supports three scheduling policies.



\### 1. FIFO (First In, First Out)



Jobs execute in the order they are submitted.



For example, if Job A arrives before Job B, Job A gets selected first.



This approach is simple, but it does not consider how important a job is.



\### 2. Fixed Priority Scheduling



Every job has a priority: High, Medium, or Low.



Higher-priority jobs are selected before lower-priority jobs.



The problem is that if high-priority jobs keep arriving, low-priority jobs may wait for a very long time. This is called \*\*starvation\*\*.



\### 3. Adaptive Priority Scheduling



This is the main feature of the project.



Instead of relying only on the original priority, the scheduler also considers how long a job has been waiting.



As a job waits longer, its effective priority increases through a technique called \*\*priority aging\*\*.



The scheduler also adjusts the aging interval based on the number of waiting jobs and gives additional priority to jobs approaching their deadlines.



This helps older and time-sensitive jobs receive attention instead of being ignored indefinitely.



\## Key Features



\- \*\*Multithreaded execution:\*\* Multiple worker threads can execute different jobs concurrently.

\- \*\*Thread-safe queue:\*\* Mutexes protect shared data from concurrent access.

\- \*\*Three scheduling policies:\*\* FIFO, Fixed Priority, and Adaptive Priority.

\- \*\*Priority aging:\*\* Waiting jobs gradually receive higher effective priority.

\- \*\*Deadline awareness:\*\* Jobs approaching or missing deadlines receive additional scheduling priority.

\- \*\*Graceful shutdown:\*\* Pending jobs finish before worker threads are joined.

\- \*\*Exception handling:\*\* A failed job does not stop the entire worker pool.

\- \*\*Performance metrics:\*\* Tracks completed jobs, failed jobs, missed deadlines, and waiting times.



\## Technologies Used



| Technology | Purpose |

|---|---|

| C++20 | Core implementation |

| STL | Containers, functions, and utilities |

| std::thread | Running multiple workers |

| std::mutex | Protecting shared data |

| std::condition\_variable | Coordinating worker threads |

| std::atomic | Thread-safe counters in benchmarks and tests |

| CMake | Building the project |

| GoogleTest | Automated unit testing |



\## Project Structure



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

|   |-- test\_threadpool.cpp

|

|-- CMakeLists.txt

|-- throughput\_results.csv

|-- README.md

```



\## How to Run the Project



Make sure you have a C++20-compatible compiler and CMake installed.



\*\*1. Build the project\*\*



```bash

cmake -S . -B build -DCMAKE\_BUILD\_TYPE=Release

cmake --build build

```



\*\*2. Run the scheduler (Windows)\*\*



```cmd

build\\scheduler.exe

```



\*\*3. Compare scheduling strategies\*\*



```cmd

build\\scheduler\_compare.exe

```



\*\*4. Run the starvation demonstration\*\*



```cmd

build\\scheduler\_starvation.exe

```



\*\*5. Measure throughput\*\*



```cmd

build\\scheduler\_throughput.exe

```



\## Performance Results



I tested the scheduler with different numbers of worker threads and jobs using a CPU-oriented synthetic workload.



Each configuration was executed five times, and the median throughput was recorded.



| Worker Threads | 10,000 Jobs | 20,000 Jobs |

|---|---:|---:|

| 1 | 20,581 jobs/sec | 4,057 jobs/sec |

| 2 | 13,707 jobs/sec | 5,594 jobs/sec |

| 4 | 30,656 jobs/sec | 29,560 jobs/sec |

| 8 | 59,360 jobs/sec | 69,202 jobs/sec |



\*\*Best observed median throughput: approximately 69,200 jobs per second\*\*, using 8 worker threads and 20,000 jobs.



These results show how worker count and scheduling overhead can affect performance. More threads do not always guarantee better results, and throughput depends on the workload and hardware.



The detailed benchmark results are available in `throughput\_results.csv`.



\## Testing



I used GoogleTest to verify the correctness of the scheduler.



The test cases cover:



\- Concurrent job execution and thread safety

\- FIFO and priority-based job ordering

\- Adaptive priority aging

\- Deadline handling

\- Job failures and exception handling

\- Graceful shutdown

\- Multiple concurrent job producers

\- Performance metrics



Run all tests using:



```bash

ctest --test-dir build --output-on-failure

```



\*\*Result: All 25 automated tests passed in the Release build.\*\*



\## What I Learned



Working on this project helped me better understand:



\- How thread pools manage concurrent tasks

\- Why mutexes and condition variables are important

\- How different scheduling policies affect job execution

\- How priority aging helps address starvation

\- Why benchmarking multithreaded systems can be challenging

\- How to test concurrency-related behavior using GoogleTest



It also gave me hands-on experience with modern C++, CMake, and building a system that combines concurrency with scheduling logic.



\## Possible Future Improvements



\- Improve job selection performance for large queues.

\- Automatically adjust the number of worker threads based on workload.

\- Add more detailed latency and fairness benchmarks.

\- Explore additional scheduling strategies.



\---



\*\*Built with C++20 to explore multithreading, scheduling algorithms, and concurrent system design.\*\*

