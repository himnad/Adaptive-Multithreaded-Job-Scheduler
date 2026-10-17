
cmake_minimum_required(VERSION 3.20)

project(AdaptiveMultithreadedJobScheduler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(scheduler
    src/main.cpp
    src/ThreadPool.cpp
)

target_include_directories(scheduler PRIVATE include)

find_package(Threads REQUIRED)
target_link_libraries(scheduler PRIVATE Threads::Threads)
