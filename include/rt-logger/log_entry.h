#pragma once

#include <rt-logger/log_level.h>

#include <array>
#include <chrono>

namespace rtlog {

struct SourceLoc {
    const char* file_;
    int line_;
    const char* function_;
};

using Timestamp = std::chrono::system_clock::time_point;

struct LogEntry {
    Timestamp timestamp_;
    LogLevel level_{};
    SourceLoc source_loc_{};
    std::array<char, 256> message_{};
};

} // namespace rtlog
