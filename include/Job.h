
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>

enum class Priority {
    Low = 1,
    Medium = 2,
    High = 3
};

struct Job {
    std::uint64_t id;
    Priority priority;
    std::function<void()> task;

    std::chrono::steady_clock::time_point submittedAt;

    std::optional<std::chrono::steady_clock::time_point> deadline =
        std::nullopt;
};
