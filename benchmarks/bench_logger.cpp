#include <rt-logger/ilog_writer.h>
#include <rt-logger/logger.h>
#include <rt-logger/mpsc_ring.h>

#include <benchmark/benchmark.h>

#include <atomic>
#include <thread>
#include <vector>

namespace rtlog {

/**
 * @brief No-op writer for benchmarking logger overhead without I/O.
 */
class NullWriter : public ILogWriter {
public:
    NullWriter() = default;
    ~NullWriter() override = default;
    NullWriter(const NullWriter&) = delete;
    NullWriter& operator=(const NullWriter&) = delete;
    NullWriter(NullWriter&&) = delete;
    NullWriter& operator=(NullWriter&&) = delete;

    std::expected<void, WriteError> write(std::string_view) noexcept override { return {}; }
};

// Measures end-to-end single-threaded log() call (timestamp + format + ring push + bg thread drain).
static void BM_LoggerSingleThread(benchmark::State& state) {
    auto ring = std::make_unique<MpscRing<1024>>();
    auto writer = std::make_unique<NullWriter>();
    Logger logger{std::move(ring), std::move(writer), LogLevel::TRACE};

    SourceLoc loc{"bench.cpp", 1, "benchmark"};

    for (auto _ : state) {
        auto result = logger.log(LogLevel::INFO, "benchmark message", loc);
        benchmark::DoNotOptimize(result);
    }

    logger.shutdown();
}
BENCHMARK(BM_LoggerSingleThread);

// Measures multi-threaded throughput: N threads each fire 10k logs; reports total items/sec.
static void BM_LoggerMultiThread(benchmark::State& state) {
    auto ring = std::make_unique<MpscRing<4096>>();
    auto writer = std::make_unique<NullWriter>();
    Logger logger{std::move(ring), std::move(writer), LogLevel::TRACE};

    SourceLoc loc{"bench.cpp", 1, "benchmark"};
    std::atomic<std::size_t> total_messages{0};

    const int num_threads = state.range(0);
    const int messages_per_thread = 10000;

    for (auto _ : state) {
        std::vector<std::jthread> threads;
        threads.reserve(num_threads);

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&logger, &loc, messages_per_thread, &total_messages] {
                for (int i = 0; i < messages_per_thread; ++i) {
                    auto result = logger.log(LogLevel::INFO, "benchmark message", loc);
                    benchmark::DoNotOptimize(result);
                }
                total_messages.fetch_add(messages_per_thread, std::memory_order_relaxed);
            });
        }

        for (auto& t : threads) {
            t.join();
        }
    }

    logger.shutdown();
    state.SetItemsProcessed(static_cast<std::int64_t>(total_messages.load()));
}
BENCHMARK(BM_LoggerMultiThread)->Arg(2)->Arg(4)->Arg(8);

// Measures the cost of a log() call that is rejected by the level filter (no ring interaction).
static void BM_LoggerFiltered(benchmark::State& state) {
    auto ring = std::make_unique<MpscRing<1024>>();
    auto writer = std::make_unique<NullWriter>();
    Logger logger{std::move(ring), std::move(writer), LogLevel::ERROR};

    SourceLoc loc{"bench.cpp", 1, "benchmark"};

    for (auto _ : state) {
        auto result = logger.log(LogLevel::INFO, "filtered message", loc);
        benchmark::DoNotOptimize(result);
    }

    logger.shutdown();
}
BENCHMARK(BM_LoggerFiltered);

} // namespace rtlog
