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

class Logger : public ILogger {
public:
    Logger(std::unique_ptr<IRing> ring, std::unique_ptr<ILogWriter> writer,
           LogLevel level = LogLevel::INFO);

    ~Logger() override;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    [[nodiscard]] std::expected<void, LoggerError> log(LogLevel level, std::string_view message,
                                                       const SourceLoc& source_loc) noexcept override;
    void set_level(LogLevel level) noexcept override;
    void set_writer(std::unique_ptr<ILogWriter> writer) noexcept override;
    void shutdown() noexcept override;

    [[nodiscard]] std::size_t write_error_count() const noexcept;

private:
    std::unique_ptr<IRing> ring_;
    std::unique_ptr<ILogWriter> writer_;
    std::mutex writer_mutex_;
    std::atomic<LogLevel> level_;
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<std::size_t> write_errors_{0};
    std::jthread thread_;

    void run(std::stop_token st);
};

} // namespace rtlog