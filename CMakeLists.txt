
cmake_minimum_required(VERSION 3.20)

project(AdaptiveMultithreadedJobScheduler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(scheduler_core
    src/ThreadPool.cpp
)

target_include_directories(scheduler_core PUBLIC include)

target_link_libraries(scheduler_core PUBLIC Threads::Threads)

add_executable(scheduler
    src/main.cpp
)

target_link_libraries(scheduler PRIVATE scheduler_core)

add_executable(scheduler_compare
    benchmarks/compare.cpp
)

target_link_libraries(scheduler_compare PRIVATE scheduler_core)

add_executable(scheduler_starvation
    benchmarks/starvation.cpp
)

target_link_libraries(scheduler_starvation PRIVATE scheduler_core)

include(CTest)

if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.16.0
    GIT_SHALLOW TRUE
)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    add_executable(scheduler_tests
        tests/test_threadpool.cpp
    )

    target_link_libraries(scheduler_tests PRIVATE
        scheduler_core
        GTest::gtest_main
    )

    include(GoogleTest)

    gtest_discover_tests(scheduler_tests)
endif()
add_executable(scheduler_throughput
    benchmarks/throughput.cpp
)

target_link_libraries(scheduler_throughput
    PRIVATE scheduler_core
)
