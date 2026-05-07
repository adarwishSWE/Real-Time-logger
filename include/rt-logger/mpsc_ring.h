#pragma once

#include <rt-logger/errors.h>
#include <rt-logger/iring.h>
#include <rt-logger/log_entry.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <new>
#include <optional>

namespace rtlog {

/**
 * @brief Lock-free multi-producer, single-consumer ring buffer.
 *
 * Supports multiple concurrent producers via CAS-based slot reservation
 * and a single consumer via try_pop. Provides both non-blocking try_push
 * and blocking push (with condition variable) operations. Shutdown
 * wakes all blocked producers with RingError::SHUTDOWN.
 *
 * @tparam ring_size Number of slots; must be a power of 2.
 */
template <std::size_t ring_size> class MpscRing : public IRing {
    static_assert((ring_size & (ring_size - 1)) == 0, "ring_size must be power of 2");
    static constexpr std::size_t mask = ring_size - 1;

    struct alignas(std::hardware_destructive_interference_size) Slot {
        alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> sequence;
        LogEntry data;
    };

    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> write_pos_{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> read_pos_{0};
    std::atomic<bool> shutdown_{false};
    std::array<Slot, ring_size> buffer_;

    std::mutex push_mutex_;
    std::condition_variable push_cv_;

public:
    /** @brief Construct an empty ring buffer with all slots initialized. */
    MpscRing() noexcept {
        for (std::size_t i = 0; i < ring_size; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    MpscRing(const MpscRing&)            = delete;
    MpscRing& operator=(const MpscRing&) = delete;
    MpscRing(MpscRing&&)                 = delete;
    MpscRing& operator=(MpscRing&&)      = delete;

    /**
     * @brief Attempt to push an entry without blocking. Thread-safe.
     *
     * Uses lock-free CAS slot reservation for concurrent producers.
     *
     * @param entry The log entry to enqueue.
     * @return Success, or RingError::FULL/RingError::SHUTDOWN.
     */
    [[nodiscard]] std::expected<void, RingError> try_push(const LogEntry& entry) noexcept override {
        if (shutdown_.load(std::memory_order_acquire)) {
            return std::unexpected(RingError::SHUTDOWN);
        }

        std::size_t pos = write_pos_.load(std::memory_order_relaxed);

        while (true) {  // LCOV_EXCL_LINE — gcov quirk: infinite-loop condition has no false branch
            const auto read_pos = read_pos_.load(std::memory_order_acquire);
            if (pos - read_pos >= ring_size) {
                return std::unexpected(RingError::FULL);
            }

            const auto index      = pos & mask;
            auto& slot            = buffer_[index];
            const std::size_t seq = slot.sequence.load(std::memory_order_acquire);

            if (seq != pos) {
                // LCOV_EXCL_START — race window: another producer reserved slot but hasn't
                // published sequence; shutdown check during that nanosecond gap is untestable
                // without instrumenting production code
                if (shutdown_.load(std::memory_order_acquire)) {
                    return std::unexpected(RingError::SHUTDOWN);
                }
                pos = write_pos_.load(std::memory_order_relaxed);
                continue;
                // LCOV_EXCL_STOP
            }

            if (write_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_acquire,
                                                 std::memory_order_relaxed)) {
                slot.data = entry;
                slot.sequence.store(pos + 1, std::memory_order_release);
                return {};
            }
        }
    }

    /**
     * @brief Attempt to pop an entry. Single-consumer only.
     *
     * @return The popped LogEntry, or std::nullopt if empty.
     */
    [[nodiscard]] std::optional<LogEntry> try_pop() noexcept override {
        const auto pos        = read_pos_.load(std::memory_order_relaxed);
        const auto index      = pos & mask;
        auto& slot            = buffer_[index];

        const std::size_t seq = slot.sequence.load(std::memory_order_acquire);
        if (seq != pos + 1) {
            return std::nullopt;
        }

        LogEntry entry = slot.data;
        slot.sequence.store(pos + ring_size, std::memory_order_release);
        read_pos_.store(pos + 1, std::memory_order_release);

        push_cv_.notify_one();

        return entry;
    }

    /**
     * @brief Push an entry, blocking until space is available. Thread-safe.
     *
     * Blocks on a condition variable when the ring is full. Returns
     * RingError::SHUTDOWN immediately if shutdown() has been called.
     *
     * @param entry The log entry to enqueue.
     * @return Success, or RingError::SHUTDOWN.
     */
    [[nodiscard]] std::expected<void, RingError> push(const LogEntry& entry) override {
        std::unique_lock lock(push_mutex_);

        while (true) {
            if (shutdown_.load(std::memory_order_acquire)) {
                return std::unexpected(RingError::SHUTDOWN);
            }

            auto result = try_push(entry);
            if (result.has_value()) {
                return result;
            }

            if (result.error() == RingError::FULL) {
                push_cv_.wait(lock, [this] {
                    if (shutdown_.load(std::memory_order_acquire)) {
                        return true;
                    }
                    const auto w = write_pos_.load(std::memory_order_relaxed);
                    const auto r = read_pos_.load(std::memory_order_acquire);
                    return w - r < ring_size;
                });

                if (shutdown_.load(std::memory_order_acquire)) {
                    return std::unexpected(RingError::SHUTDOWN);
                }
            } else {
                return result;  // LCOV_EXCL_LINE — race: try_push returns SHUTDOWN (not FULL)
                                // between push's shutdown check and try_push call
            }
        }
    }

    /**
     * @brief Signal shutdown to all blocked producers. Idempotent.
     *
     * Wakes all threads blocked in push() via notify_all().
     * After shutdown, try_push/push return RingError::SHUTDOWN.
     * try_pop() continues to drain remaining entries.
     */
    void shutdown() noexcept override {
        shutdown_.store(true, std::memory_order_release);
        push_cv_.notify_all();
    }
};

}  // namespace rtlog
