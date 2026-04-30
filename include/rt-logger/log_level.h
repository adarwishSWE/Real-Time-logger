#pragma once

#include <ostream>
#include <string_view>

namespace rtlog {

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
};

[[nodiscard]] constexpr std::string_view to_string(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
    }
    return "UNKNOWN";
}

inline std::ostream& operator<<(std::ostream& os, LogLevel level) noexcept {
    return os << to_string(level);
}

} // namespace rtlog