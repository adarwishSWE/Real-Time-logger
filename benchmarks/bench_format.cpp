#include <rt-logger/log_level.h>
#include <rt-logger/log_entry.h>

#include <benchmark/benchmark.h>

#include <array>
#include <chrono>
#include <cstring>
#include <ctime>
#include <format>
#include <iterator>
#include <string_view>

namespace rtlog {

// Measures LogLevel-to-string-view lookup (hot path in formatting).
static void BM_ToString(benchmark::State& state) {
    LogLevel level = LogLevel::INFO;
    for (auto benchmark_iter : state) {
        auto level_str = to_string(level);
        benchmark::DoNotOptimize(level_str);
    }
}
BENCHMARK(BM_ToString);

namespace {

std::string_view format_bench_entry(const LogEntry& entry, std::array<char, 512>& buf) {
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

// Measures full log-entry formatting with a short message and short source location.
// NOTE: This reimplements format_entry from logger.cpp because it lives in an anonymous
// namespace. If format_entry is ever extracted to a public header, this should call it
// directly instead.
static void BM_SimpleFormat(benchmark::State& state) {
    LogEntry entry{};
    entry.timestamp_ = std::chrono::system_clock::now();
    entry.level_ = LogLevel::INFO;
    entry.source_loc_ = {.file_ = "bench.cpp", .line_ = 42, .function_ = "benchmark"};
    std::memcpy(entry.message_.data(), "simple benchmark message", 25);

    std::array<char, 512> buf{};

    for (auto benchmark_iter : state) {
        std::string_view formatted = format_bench_entry(entry, buf);
        benchmark::DoNotOptimize(formatted);
    }
}
BENCHMARK(BM_SimpleFormat);

// Measures full log-entry formatting with a long message and long source path.
// NOTE: See BM_SimpleFormat for why this reimplements format_entry.
static void BM_LongMessageFormat(benchmark::State& state) {
    LogEntry entry{};
    entry.timestamp_ = std::chrono::system_clock::now();
    entry.level_ = LogLevel::ERROR;
    entry.source_loc_ = {.file_ = "very/long/path/to/source/file.cpp",
        .line_ = 999,
        .function_ = "some_function_name"};
    std::string long_msg(200, 'x');
    std::memcpy(entry.message_.data(),
        long_msg.data(),
        std::min(long_msg.size(), entry.message_.size() - 1));

    std::array<char, 512> buf{};

    for (auto benchmark_iter : state) {
        std::string_view formatted = format_bench_entry(entry, buf);
        benchmark::DoNotOptimize(formatted);
    }
}
BENCHMARK(BM_LongMessageFormat);

} // namespace rtlog
