#pragma once

#include <rt-logger/log_level.h>

#include <array>
#include <chrono>

namespace rtlog {

struct SourceLoc {
    const char* file;
    int line;
    const char* function;
};

using Timestamp = std::chrono::system_clock::time_point;

struct LogEntry {
    Timestamp timestamp;
    LogLevel level;
    SourceLoc source_loc;
    std::array<char, 256> message;
};

}  // namespace rtlog
