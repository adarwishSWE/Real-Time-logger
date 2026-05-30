#pragma once

#include <cstdint>
#include <ostream>
#include <string_view>

namespace rtlog {

enum class LogLevel : std::uint8_t {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
};

constexpr std::string_view to_string(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::TRACE:
        return "TRACE";
    case LogLevel::DEBUG:
        return "DEBUG";
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::WARN:
        return "WARN";
    case LogLevel::ERROR:
        return "ERROR";
    case LogLevel::FATAL:
        return "FATAL";
    }
    return "UNKNOWN";
}

inline std::ostream& operator<<(std::ostream& ostream, LogLevel level) noexcept {
    return ostream << to_string(level);
}

} // namespace rtlog
