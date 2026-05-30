#pragma once

#include <rt-logger/ilog_writer.h>
#include <rt-logger/ilogger.h>
#include <rt-logger/iring.h>
#include <rt-logger/log_entry.h>
#include <rt-logger/log_level.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>

namespace rtlog {

/**
 * @brief Thread-safe asynchronous logger with a background consumer thread.
 *
 * Accepts log entries from any thread via log(), enqueues them into an
 * MPSC ring buffer, and dispatches formatted output to an ILogWriter on
 * a dedicated background thread. Supports dynamic level filtering, writer
 * switching, write-error tracking, and graceful drain-on-shutdown.
 *
 * @section Shutdown Contract
 *
 * 1. **Pending messages are guaranteed to be flushed.** After shutdown() is
 *    called, the consumer thread drains every remaining entry from the ring
 *    before exiting.
 * 2. **Messages racing with shutdown are rejected.** If log() is called
 *    concurrently with shutdown(), it may either succeed (message enqueued
 *    before the ring is shut down) or fail with ALREADY_SHUTDOWN.
 * 3. **log() after shutdown always fails immediately.** Once shutdown() has
 *    completed, all subsequent log() calls return ALREADY_SHUTDOWN.
 */
class Logger : public ILogger {
public:
    /**
     * @brief Construct a logger with the given ring buffer, writer, and minimum level.
     *
     * Starts the background consumer thread immediately.
     *
     * @param ring    Ownership of the ring buffer implementation.
     * @param writer  Ownership of the initial output writer.
     * @param level   Minimum log level (messages below this are discarded).
     */
    Logger(std::unique_ptr<IRing> ring, std::unique_ptr<ILogWriter> writer,
        LogLevel level = LogLevel::INFO);

    /**
     * @brief Destructor. Calls shutdown() if not already called.
     */
    ~Logger() override;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    /**
     * @brief Submit a log entry. Thread-safe.
     *
     * Applies level filtering before enqueueing. If the logger has been
     * shut down, returns LoggerError::ALREADY_SHUTDOWN.
     *
     * @param level       Severity level of the message.
     * @param message    Log message text (truncated to 255 chars).
     * @param source_loc Source location (file, line, function).
     * @return Success, or LoggerError::ALREADY_SHUTDOWN.
     */
    std::expected<void, LoggerError> log(LogLevel level, std::string_view message,
        const SourceLoc& source_loc) noexcept override;

    /**
     * @brief Change the minimum log level. Thread-safe.
     *
     * @param level The new minimum severity level.
     */
    void set_level(LogLevel level) noexcept override;

    /**
     * @brief Replace the output writer. Thread-safe.
     *
     * Takes ownership. The old writer is destroyed once any in-progress
     * writes complete.
     *
     * @param writer The new writer (ownership transferred).
     */
    void set_writer(std::unique_ptr<ILogWriter> writer) noexcept override;

    /**
     * @brief Shut down the logger gracefully. Thread-safe and idempotent.
     *
     * Signals the background thread to stop, drains remaining entries,
     * and joins the thread.
     */
    void shutdown() noexcept override;

    /**
     * @brief Get the number of write failures since construction.
     *
     * Incremented each time the background thread's write() call fails.
     *
     * @return The cumulative write error count.
     */
    std::size_t write_error_count() const noexcept;

private:
    std::unique_ptr<IRing> ring_;
    std::unique_ptr<ILogWriter> writer_;
    std::mutex writer_mutex_;
    std::atomic<LogLevel> level_;
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<std::size_t> write_errors_{0};
    std::jthread thread_;

    /** @brief Background consumer loop. Formats and writes entries. */
    void run(std::stop_token st);
};

} // namespace rtlog
