#pragma once

namespace rtlog {

enum class WriteError {
    FILE_OPEN_FAILED,
    WRITE_FAILED,
};

enum class RingError {
    FULL,
    EMPTY,
    SHUTDOWN,
};

enum class LoggerError {
    ALREADY_SHUTDOWN,
};

} // namespace rtlog