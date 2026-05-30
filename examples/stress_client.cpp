/**
 * @file stress_client.cpp
 * @brief High-throughput burn-in test for rt-logger using FileWriter.
 *
 * 4 threads × 250,000 messages each, ring size 4096, INFO level.
 * Reports total elapsed time and throughput (msg/sec).
 */

#include <rt-logger/file_writer.h>
#include <rt-logger/logger.h>
#include <rt-logger/mpsc_ring.h>

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main() {
    constexpr int num_threads = 4;
    constexpr int messages_per_thread = 250'000;
    constexpr int ring_size = 4096;
    constexpr rtlog::LogLevel min_level = rtlog::LogLevel::INFO;

    const std::string log_file = "stress_client.log";

    auto ring = std::make_unique<rtlog::MpscRing<ring_size>>();
    auto writer_or_err = rtlog::FileWriter::create(log_file);
    if (!writer_or_err) {
        std::cerr << "Failed to create log file: " << log_file << "\n";
        return EXIT_FAILURE;
    }

    rtlog::Logger logger{std::move(ring), std::move(writer_or_err), min_level};

    rtlog::SourceLoc loc{"stress_client.cpp", __LINE__, __func__};

    std::cout << "Stress test: " << num_threads << " threads × " << messages_per_thread
              << " messages (ring=" << ring_size << ", level=INFO)\n";

    const auto start = std::chrono::steady_clock::now();

    std::vector<std::jthread> threads;
    threads.reserve(num_threads);

    for (int thread_idx = 0; thread_idx < num_threads; ++thread_idx) {
        threads.emplace_back([&logger, &loc] {
            for (int msg_idx = 0; msg_idx < messages_per_thread; ++msg_idx) {
                logger.log(rtlog::LogLevel::INFO, "Thread message", loc);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    logger.shutdown();

    const auto end = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    const auto total_ms = elapsed.count();
    const std::size_t total_messages = static_cast<std::size_t>(num_threads) * messages_per_thread;

    double seconds = total_ms > 0 ? static_cast<double>(total_ms) / 1000.0 : 1.0;
    double throughput = static_cast<double>(total_messages) / seconds;

    std::cout << "Elapsed:  " << total_ms << " ms\n";
    std::cout << "Messages: " << total_messages << "\n";
    std::cout << "Throughput: " << static_cast<std::size_t>(throughput) << " msg/sec\n";
    std::cout << "Log written to: " << log_file << "\n";

    return 0;
}
