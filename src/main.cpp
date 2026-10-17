
#include "ThreadPool.h"

#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

int main() {
    std::mutex outputMutex;

    {
        ThreadPool pool(4);

        for (int i = 1; i <= 10; ++i) {
            pool.enqueue([i, &outputMutex] {
                {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    std::cout << "Job " << i
                              << " started by thread "
                              << std::this_thread::get_id()
                              << '\n';
                }

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(1000)
                );

                {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    std::cout << "Job " << i
                              << " completed\n";
                }
            });
        }
    }

    std::cout << "All jobs completed!\n";
    return 0;
}
