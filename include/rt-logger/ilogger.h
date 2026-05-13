#pragma once

#include <rt-logger/errors.h>
#include <rt-logger/ilog_writer.h>
#include <rt-logger/log_entry.h>

#include <expected>
#include <memory>
#include <string_view>

namespace rtlog {

/**
 * @brief Abstract interface for logger components.
 *
 * Defines the contract for thread-safe loggers that accept log entries,
 * manage output writers, and support graceful shutdown. All public methods
 * are safe to call from any thread.
 */
class ILogger {
public:
    ILogger() = default;
    // LCOV_EXCL_START — deleting destructor of abstract class is structurally unreachable
    virtual ~ILogger() = default;
    // LCOV_EXCL_STOP
    ILogger(const ILogger&)            = delete;
    ILogger& operator=(const ILogger&) = delete;
    ILogger(ILogger&&)                 = delete;
    ILogger& operator=(ILogger&&)      = delete;

    /**
     * @brief Submit a log entry to the logger.
     *
     * Applies level filtering before enqueueing. If the logger has been
     * shut down, returns LoggerError::ALREADY_SHUTDOWN. This method is
     * safe to call from any thread.
     *
     * @param level      Severity level of the message.
     * @param message    Log message text.
     * @param source_loc Source location (file, line, function).
     * @return Success, or LoggerError::ALREADY_SHUTDOWN if the logger is shut down.
     */
    virtual std::expected<void, LoggerError>
    log(LogLevel level, std::string_view message, const SourceLoc& source_loc) noexcept = 0;

    /**
     * @brief Change the minimum log level at runtime.
     *
     * Messages below the new level will be silently discarded by log().
     * This method is thread-safe.
     *
     * @param level The new minimum severity level.
     */
    virtual void set_level(LogLevel level) noexcept = 0;

    /**
     * @brief Replace the output writer.
     *
     * Takes ownership of the new writer. The old writer is destroyed once
     * any in-progress writes complete. Synchronized internally with the
     * consumer thread via a mutex so that no write is interrupted mid-flight.
     * This method is thread-safe.
     *
     * @param writer The new writer (ownership transferred).
     */
    virtual void set_writer(std::unique_ptr<ILogWriter> writer) noexcept = 0;

    /**
     * @brief Shut down the logger gracefully.
     *
     * Signals the background thread to stop, drains remaining entries from
     * the ring buffer, and joins the thread. Idempotent — safe to call
     * multiple times.
     */
    virtual void shutdown() noexcept = 0;
};

}  // namespace rtlog
