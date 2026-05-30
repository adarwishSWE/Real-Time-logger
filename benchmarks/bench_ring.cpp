#include <rt-logger/log_entry.h>
#include <rt-logger/mpsc_ring.h>

#include <benchmark/benchmark.h>

#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

namespace rtlog {

static LogEntry make_bench_entry() {
    LogEntry entry{};
    entry.timestamp_ = std::chrono::system_clock::now();
    entry.level_ = LogLevel::INFO;
    entry.source_loc_ = {.file_ = "bench.cpp", .line_ = 1, .function_ = "bench"};
    std::memcpy(entry.message_.data(), "benchmark", 10);
    return entry;
}

// Measures the cost of a single-threaded try_push followed by try_pop cycle.
static void BM_RingTryPushPop(benchmark::State& state) {
    MpscRing<1024> ring;
    auto entry = make_bench_entry();

    for (auto benchmark_iter : state) {
        auto result = ring.try_push(entry);
        benchmark::DoNotOptimize(result);
        ring.try_pop();
    }
}
BENCHMARK(BM_RingTryPushPop);

// Measures multi-threaded try_push contention. Iterations represent push attempts, not
// successes — once the ring saturates, try_push returns RingError::FULL. Useful for
// comparing CAS overhead at different thread counts, not for throughput measurement.
static void BM_RingTryPushContended(benchmark::State& state) {
    MpscRing<4096> ring;
    auto entry = make_bench_entry();

    for (auto benchmark_iter : state) {
        auto result = ring.try_push(entry);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_RingTryPushContended)->ThreadRange(2, 8);

// Measures try_pop when the ring is pre-filled, re-filling after each pop to keep steady state.
static void BM_RingTryPop(benchmark::State& state) {
    MpscRing<1024> ring;
    auto entry = make_bench_entry();

    // Pre-fill the ring
    for (int fill_idx = 0; fill_idx < 512; ++fill_idx) {
        static_cast<void>(ring.try_push(entry));
    }

    for (auto benchmark_iter : state) {
        auto result = ring.try_pop();
        benchmark::DoNotOptimize(result);
        if (result.has_value()) {
            static_cast<void>(ring.try_push(entry));
        }
    }
}
BENCHMARK(BM_RingTryPop);

// Measures blocking push with a concurrent consumer draining the ring (producer-only timing).
static void BM_RingPushBlocking(benchmark::State& state) {
    MpscRing<64> ring;
    auto entry = make_bench_entry();
    std::atomic<bool> running{true};

    // Consumer thread to drain the ring
    std::jthread consumer([&ring, &running] {
        while (running.load(std::memory_order_acquire)) {
            auto result = ring.try_pop();
            if (!result.has_value()) {
                std::this_thread::yield();
            }
        }
    });

    for (auto benchmark_iter : state) {
        auto result = ring.push(entry);
        benchmark::DoNotOptimize(result);
    }

    running.store(false, std::memory_order_release);
}
BENCHMARK(BM_RingPushBlocking);

} // namespace rtlog
