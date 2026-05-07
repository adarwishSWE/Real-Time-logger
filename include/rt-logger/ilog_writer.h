#pragma once

#include <rt-logger/errors.h>

#include <expected>
#include <string_view>

namespace rtlog {

/**
 * @brief Abstract interface for log output destinations.
 *
 * Implementations write formatted log lines to specific targets
 * (console, file, network, etc.). All write operations must be
 * thread-safe if shared across threads.
 */
class ILogWriter {
public:
    // LCOV_EXCL_START — deleting destructor of abstract class is structurally unreachable
    virtual ~ILogWriter() = default;
    // LCOV_EXCL_STOP
    ILogWriter(const ILogWriter&)            = delete;
    ILogWriter& operator=(const ILogWriter&) = delete;
    ILogWriter(ILogWriter&&)                 = delete;
    ILogWriter& operator=(ILogWriter&&)      = delete;

    /**
     * @brief Write a formatted log line to the output destination.
     *
     * @param message The fully formatted log line (including newline).
     * @return Success, or a WriteError on failure.
     */
    [[nodiscard]] virtual std::expected<void, WriteError>
    write(std::string_view message) noexcept = 0;

protected:
    ILogWriter() = default;
};

}  // namespace rtlog
