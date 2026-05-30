#include <rt-logger/logger.h>

#include <array>
#include <cassert>
#include <chrono>
#include <cstring>
#include <ctime>
#include <expected>
#include <format>
#include <iostream>
#include <iterator>
#include <string_view>

namespace rtlog {

namespace {

std::string_view format_entry(const LogEntry& entry, std::array<char, 512>& buf) {
    const auto time_t_val = std::chrono::system_clock::to_time_t(entry.timestamp_);
    const auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(entry.timestamp_.time_since_epoch()) %
        std::chrono::milliseconds(1000);
    std::tm tm_buf{};
    localtime_r(&time_t_val, &tm_buf);

    std::array<char, 32> time_prefix{};
    const std::size_t time_len =
        std::strftime(time_prefix.data(), time_prefix.size(), "[%Y-%m-%d %H:%M:%S", &tm_buf);

    const char* file = entry.source_loc_.file_ != nullptr ? entry.source_loc_.file_ : "";
    const char* function =
        entry.source_loc_.function_ != nullptr ? entry.source_loc_.function_ : "";
    const auto message_len = std::strlen(entry.message_.data());

    const auto formatted = std::format_to_n(buf.begin(),
        static_cast<std::ptrdiff_t>(buf.size()),
        "{}.{:03}] [{}] {}:{} ({}) — {}",
        std::string_view(time_prefix.data(), time_len),
        millis.count(),
        to_string(entry.level_),
        file,
        entry.source_loc_.line_,
        function,
        std::string_view(entry.message_.data(), message_len));

    const auto length = static_cast<std::size_t>(formatted.out - buf.data());
    return {buf.data(), std::min(length, buf.size())};
}

} // anonymous namespace

Logger::Logger(std::unique_ptr<IRing> ring, std::unique_ptr<ILogWriter> writer, LogLevel level)
    : ring_{std::move(ring)}, writer_{std::move(writer)}, level_{level} {
    thread_ = std::jthread{&Logger::run, this};
}

Logger::~Logger() {
    shutdown_impl();
}

std::expected<void, LoggerError> Logger::log(LogLevel level, std::string_view message,
    const SourceLoc& source_loc) noexcept {
    if (shutdown_requested_.load(std::memory_order_acquire)) {
        return std::unexpected(LoggerError::ALREADY_SHUTDOWN);
    }

    assert(message.data() != nullptr);

    if (static_cast<int>(level) < static_cast<int>(level_.load(std::memory_order_relaxed))) {
        return {};
    }

    LogEntry entry{};
    entry.timestamp_ = std::chrono::system_clock::now();
    entry.level_ = level;
    entry.source_loc_ = source_loc;

    const std::size_t copy_len = std::min(message.size(), entry.message_.size() - 1);
    std::memcpy(entry.message_.data(), message.data(), copy_len);
    entry.message_[copy_len] = '\0';

    auto result = ring_->push(entry);
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
    shutdown_impl();
}

void Logger::shutdown_impl() noexcept {
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

void Logger::run(const std::stop_token& stop_token) {
    std::array<char, 512> buf{};

    while (!stop_token.stop_requested()) {
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

} // namespace rtlog
