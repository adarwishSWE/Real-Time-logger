#include <rt-logger/log_entry.h>
#include <rt-logger/mpsc_ring.h>

#include <benchmark/benchmark.h>

#include <atomic>
#include <cstring>
#include <thread>

namespace rtlog {

static LogEntry make_bench_entry() {
    LogEntry entry{};
    entry.timestamp_ = std::chrono::system_clock::now();
    entry.level_ = LogLevel::INFO;
    entry.source_loc_ = {.file_ = "bench.cpp", .line_ = 1, .function_ = "bench"};
    std::memcpy(entry.message_.data(), "benchmark", 10);
    return entry;
}

// Lock-free hot path: try_push followed by try_pop on an empty ring.
static void BM_RingTryPushPop(benchmark::State& state) {
    MpscRing<1024> ring;
    auto entry = make_bench_entry();

    for (auto benchmark_iter : state) {
        auto push_result = ring.try_push(entry);
        benchmark::DoNotOptimize(push_result);
        auto pop_result = ring.try_pop();
        benchmark::DoNotOptimize(pop_result);
    }
}
BENCHMARK(BM_RingTryPushPop);

// push() on a non-full ring: try_push fast path plus std::expected wrapper overhead.
static void BM_RingPushWhenNotFull(benchmark::State& state) {
    MpscRing<1024> ring;
    auto entry = make_bench_entry();

    for (auto benchmark_iter : state) {
        auto push_result = ring.push(entry);
        benchmark::DoNotOptimize(push_result);
        auto pop_result = ring.try_pop();
        benchmark::DoNotOptimize(pop_result);
    }
}
BENCHMARK(BM_RingPushWhenNotFull);

// push() on a full ring; consumer pops on a fixed cadence without refill.
static void BM_RingPushBlocksOnFull(benchmark::State& state) {
    MpscRing<1> ring;
    auto entry = make_bench_entry();
    auto prefill_result = ring.try_push(entry);
    benchmark::DoNotOptimize(prefill_result);

    std::atomic<bool> running{true};
    std::jthread consumer([&ring, &running] {
        while (running.load(std::memory_order_acquire)) {
            auto pop_result = ring.try_pop();
            benchmark::DoNotOptimize(pop_result);
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    });

    for (auto benchmark_iter : state) {
        auto push_result = ring.push(entry);
        benchmark::DoNotOptimize(push_result);
    }

    running.store(false, std::memory_order_release);
    static_cast<void>(ring.try_pop());
}
BENCHMARK(BM_RingPushBlocksOnFull);

// Multi-threaded try_push when the ring is saturated: measures FULL early-exit under
// read_pos_ cache-line contention, not CAS reservation work.
static void BM_RingTryPushContendedFull(benchmark::State& state) {
    MpscRing<4096> ring;
    auto entry = make_bench_entry();

    for (auto benchmark_iter : state) {
        auto result = ring.try_push(entry);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_RingTryPushContendedFull)->ThreadRange(2, 8);

// Multi-threaded try_push with a single consumer draining; exercises CAS contention.
static void BM_RingTryPushContendedDrained(benchmark::State& state) {
    MpscRing<4096> ring;
    auto entry = make_bench_entry();
    std::atomic<bool> running{true};

    std::jthread consumer([&ring, &running] {
        while (running.load(std::memory_order_acquire)) {
            auto pop_result = ring.try_pop();
            if (!pop_result.has_value()) {
                std::this_thread::yield();
            }
        }
    });

    for (auto benchmark_iter : state) {
        auto result = ring.try_push(entry);
        benchmark::DoNotOptimize(result);
    }

    running.store(false, std::memory_order_release);
    while (ring.try_pop().has_value()) {
    }
}
BENCHMARK(BM_RingTryPushContendedDrained)->ThreadRange(2, 8);

// try_pop on a half-filled ring, re-filling after each pop to keep steady state.
static void BM_RingTryPop(benchmark::State& state) {
    MpscRing<1024> ring;
    auto entry = make_bench_entry();

    for (int fill_idx = 0; fill_idx < 512; ++fill_idx) {
        auto result = ring.try_push(entry);
        benchmark::DoNotOptimize(result);
    }

    for (auto benchmark_iter : state) {
        auto pop_result = ring.try_pop();
        benchmark::DoNotOptimize(pop_result);
        if (pop_result.has_value()) {
            auto refill_result = ring.try_push(entry);
            benchmark::DoNotOptimize(refill_result);
        }
    }
}
BENCHMARK(BM_RingTryPop);

// push() with a concurrent drainer on a mid-size ring (producer-only timing).
static void BM_RingPushWithConsumer(benchmark::State& state) {
    MpscRing<64> ring;
    auto entry = make_bench_entry();
    std::atomic<bool> running{true};

    std::jthread consumer([&ring, &running] {
        while (running.load(std::memory_order_acquire)) {
            auto pop_result = ring.try_pop();
            if (!pop_result.has_value()) {
                std::this_thread::yield();
            }
        }
    });

    for (auto benchmark_iter : state) {
        auto push_result = ring.push(entry);
        benchmark::DoNotOptimize(push_result);
    }

    running.store(false, std::memory_order_release);
}
BENCHMARK(BM_RingPushWithConsumer);

} // namespace rtlog
