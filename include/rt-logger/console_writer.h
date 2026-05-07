#pragma once

#include <rt-logger/ilog_writer.h>

#include <iostream>

namespace rtlog {

/**
 * @brief Writes formatted log lines to stdout.
 *
 * Thin wrapper around std::cout. Not thread-safe on its own; thread safety
 * is provided by the Logger's single-consumer design (only the background
 * thread calls write()).
 */
class ConsoleWriter : public ILogWriter {
public:
    ConsoleWriter()                                = default;
    ~ConsoleWriter() override                      = default;
    ConsoleWriter(const ConsoleWriter&)            = default;
    ConsoleWriter& operator=(const ConsoleWriter&) = default;
    ConsoleWriter(ConsoleWriter&&)                 = default;
    ConsoleWriter& operator=(ConsoleWriter&&)      = default;

    /**
     * @brief Write a formatted log line to stdout, followed by a newline.
     *
     * @param message The formatted log line.
     * @return Success, or WriteError::WRITE_FAILED if the stream is in a bad state.
     */
    [[nodiscard]] std::expected<void, WriteError> write(std::string_view message) noexcept override;
};

}  // namespace rtlog
