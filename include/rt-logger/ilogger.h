#pragma once

#include <rt-logger/errors.h>
#include <rt-logger/ilog_writer.h>
#include <rt-logger/log_entry.h>

#include <expected>
#include <memory>
#include <string_view>

namespace rtlog {

class ILogger {
public:
    ILogger() = default;
    virtual ~ILogger() = default;
    ILogger(const ILogger&) = delete;
    ILogger& operator=(const ILogger&) = delete;
    ILogger(ILogger&&) = delete;
    ILogger& operator=(ILogger&&) = delete;

    [[nodiscard]] virtual std::expected<void, LoggerError> log(LogLevel level, std::string_view message, const SourceLoc& source_loc) noexcept = 0;
    virtual void set_level(LogLevel level) noexcept = 0;
    virtual void set_writer(std::unique_ptr<ILogWriter> writer) noexcept = 0;
    virtual void shutdown() noexcept = 0;
};

} // namespace rtlog