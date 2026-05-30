#pragma once

#include <cstdint>

namespace rtlog {

enum class WriteError : std::uint8_t {
    FILE_OPEN_FAILED,
    WRITE_FAILED,
};

enum class RingError : std::uint8_t {
    FULL,
    EMPTY,
    SHUTDOWN,
};

enum class LoggerError : std::uint8_t {
    ALREADY_SHUTDOWN,
};

} // namespace rtlog
