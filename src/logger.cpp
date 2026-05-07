#include <rt-logger/logger.h>

#include <array>
#include <chrono>
#include <ctime>
#include <cstring>
#include <expected>
#include <iostream>
#include <string_view>

namespace rtlog {

namespace {

std::string_view format_entry(const LogEntry& entry, std::array<char, 512>& buf) {
    auto time_t_val = std::chrono::system_clock::to_time_t(entry.timestamp);
    auto ms         = std::chrono::duration_cast<std::chrono::milliseconds>(
                  entry.timestamp
                      .time_since_epoch()) %  // LCOV_EXCL_LINE — gcov quirk: multi-line expression
                                                      // continuation reported as separate line
              1000;
    std::tm tm_buf{};
    localtime_r(&time_t_val, &tm_buf);

    std::size_t offset = std::strftime(buf.data(), buf.size(), "[%Y-%m-%d %H:%M:%S", &tm_buf);

    int remaining      = static_cast<int>(buf.size() - offset);
    if (remaining > 0) {
        int n = std::snprintf(
            buf.data() + offset, static_cast<std::size_t>(remaining),
            ".%03ld] [%s] %s:%d (%s) — %s", static_cast<long>(ms.count()),
            to_string(entry.level).data(), entry.source_loc.file ? entry.source_loc.file : "",
            entry.source_loc.line, entry.source_loc.function ? entry.source_loc.function : "",
            entry.message.data());
        if (n > 0) {
            offset += static_cast<std::size_t>(n);
        }
    }

    if (offset > buf.size()) {
        offset = buf.size();
    }

    return std::string_view{buf.data(), offset};
}

}  // anonymous namespace

Logger::Logger(std::unique_ptr<IRing> ring, std::unique_ptr<ILogWriter> writer, LogLevel level)
    : ring_{std::move(ring)}, writer_{std::move(writer)}, level_{level} {
    thread_ = std::jthread{&Logger::run, this};
}

Logger::~Logger() {
    shutdown();
}

std::expected<void, LoggerError>
Logger::log(LogLevel level, std::string_view message, const SourceLoc& source_loc) noexcept {
    if (shutdown_requested_.load(std::memory_order_acquire)) {
        return std::unexpected(LoggerError::ALREADY_SHUTDOWN);
    }

    if (static_cast<int>(level) < static_cast<int>(level_.load(std::memory_order_relaxed))) {
        return {};
    }

    LogEntry entry{};
    entry.timestamp            = std::chrono::system_clock::now();
    entry.level                = level;
    entry.source_loc           = source_loc;

    const std::size_t copy_len = std::min(message.size(), entry.message.size() - 1);
    std::memcpy(entry.message.data(), message.data(), copy_len);
    entry.message[copy_len] = '\0';

    auto result             = ring_->push(entry);
    // LCOV_EXCL_START — race: ring shuts down between log()'s check and push()
    if (!result.has_value() && result.error() == RingError::SHUTDOWN) {
        return std::unexpected(LoggerError::ALREADY_SHUTDOWN);
    }
    // LCOV_EXCL_STOP

    return {};
}

void Logger::set_level(LogLevel level) noexcept {
    level_.store(level, std::memory_order_relaxed);
}

void Logger::set_writer(std::unique_ptr<ILogWriter> writer) noexcept {
    std::lock_guard lock{writer_mutex_};
    writer_ = std::move(writer);
}

void Logger::shutdown() noexcept {
    bool expected = false;
    if (!shutdown_requested_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    ring_->shutdown();
    thread_.request_stop();
    thread_.join();
}

std::size_t Logger::write_error_count() const noexcept {
    return write_errors_.load(std::memory_order_relaxed);
}

void Logger::run(std::stop_token st) {
    std::array<char, 512> buf{};

    while (!st.stop_requested()) {
        auto entry = ring_->try_pop();
        if (entry.has_value()) {
            auto formatted = format_entry(*entry, buf);

            std::lock_guard lock{writer_mutex_};
            auto result = writer_->write(formatted);
            if (!result.has_value()) {
                write_errors_.fetch_add(1, std::memory_order_relaxed);
                std::cerr << "[rt-logger] Write failed\n";
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    while (true) {
        auto entry = ring_->try_pop();
        if (!entry.has_value()) {
            break;
        }

        auto formatted = format_entry(*entry, buf);

        std::lock_guard lock{writer_mutex_};
        auto result = writer_->write(formatted);
        if (!result.has_value()) {
            write_errors_.fetch_add(1, std::memory_order_relaxed);
            std::cerr << "[rt-logger] Write failed\n";
        }
    }
}

}  // namespace rtlog
