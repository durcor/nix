#pragma once
///@file

#include <chrono>
#include <cstdint>
#include <ctime>
#include <optional>

namespace nix {

struct BuildResourceUsage
{
    std::optional<uint64_t> peakMemoryBytes;
    std::optional<std::chrono::microseconds> cpuUser;
    std::optional<std::chrono::microseconds> cpuSystem;
    std::optional<time_t> wallTime;
    time_t sampleTime = 0;
};

} // namespace nix
