#include <rt-logger/log_level.h>
#include <rt-logger/log_entry.h>

#include <benchmark/benchmark.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace rtlog {

// Measures LogLevel-to-string-view lookup (hot path in formatting).
static void BM_ToString(benchmark::State& state) {
    LogLevel level = LogLevel::INFO;
    for (auto _ : state) {
        auto sv = to_string(level);
        benchmark::DoNotOptimize(sv);
    }
}
BENCHMARK(BM_ToString);

// Measures full log-entry formatting with a short message and short source location.
// NOTE: This reimplements format_entry from logger.cpp because it lives in an anonymous
// namespace. If format_entry is ever extracted to a public header, this should call it
// directly instead.
static void BM_SimpleFormat(benchmark::State& state) {
    LogEntry entry{};
    entry.timestamp  = std::chrono::system_clock::now();
    entry.level      = LogLevel::INFO;
    entry.source_loc = {"bench.cpp", 42, "benchmark"};
    std::memcpy(entry.message.data(), "simple benchmark message", 25);

    std::array<char, 512> buf{};

    for (auto _ : state) {
        auto time_t_val = std::chrono::system_clock::to_time_t(entry.timestamp);
        auto ms         = std::chrono::duration_cast<std::chrono::milliseconds>(
                      entry.timestamp.time_since_epoch()) %
                  1000;
        std::tm tm_buf{};
        localtime_r(&time_t_val, &tm_buf);
        std::size_t offset = std::strftime(buf.data(), buf.size(), "[%Y-%m-%d %H:%M:%S", &tm_buf);

        int n              = std::snprintf(
            buf.data() + offset, buf.size() - offset, ".%03ld] [%s] %s:%d (%s) — %s",
            static_cast<long>(ms.count()), to_string(entry.level).data(),
            entry.source_loc.file ? entry.source_loc.file : "", entry.source_loc.line,
            entry.source_loc.function ? entry.source_loc.function : "", entry.message.data());
        if (n > 0) {
            offset += static_cast<std::size_t>(n);
        }

        std::string_view formatted{buf.data(), offset};
        benchmark::DoNotOptimize(formatted);
    }
}
BENCHMARK(BM_SimpleFormat);

// Measures full log-entry formatting with a long message and long source path.
// NOTE: See BM_SimpleFormat for why this reimplements format_entry.
static void BM_LongMessageFormat(benchmark::State& state) {
    LogEntry entry{};
    entry.timestamp  = std::chrono::system_clock::now();
    entry.level      = LogLevel::ERROR;
    entry.source_loc = {"very/long/path/to/source/file.cpp", 999, "some_function_name"};
    std::string long_msg(200, 'x');
    std::memcpy(entry.message.data(), long_msg.data(),
                std::min(long_msg.size(), entry.message.size() - 1));

    std::array<char, 512> buf{};

    for (auto _ : state) {
        auto time_t_val = std::chrono::system_clock::to_time_t(entry.timestamp);
        auto ms         = std::chrono::duration_cast<std::chrono::milliseconds>(
                      entry.timestamp.time_since_epoch()) %
                  1000;
        std::tm tm_buf{};
        localtime_r(&time_t_val, &tm_buf);
        std::size_t offset = std::strftime(buf.data(), buf.size(), "[%Y-%m-%d %H:%M:%S", &tm_buf);

        int n              = std::snprintf(
            buf.data() + offset, buf.size() - offset, ".%03ld] [%s] %s:%d (%s) — %s",
            static_cast<long>(ms.count()), to_string(entry.level).data(),
            entry.source_loc.file ? entry.source_loc.file : "", entry.source_loc.line,
            entry.source_loc.function ? entry.source_loc.function : "", entry.message.data());
        if (n > 0) {
            offset += static_cast<std::size_t>(n);
        }

        std::string_view formatted{buf.data(), offset};
        benchmark::DoNotOptimize(formatted);
    }
}
BENCHMARK(BM_LongMessageFormat);

}  // namespace rtlog
