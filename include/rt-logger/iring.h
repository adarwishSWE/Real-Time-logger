#pragma once

#include <rt-logger/errors.h>
#include <rt-logger/log_entry.h>

#include <expected>
#include <optional>

namespace rtlog {

/**
 * @brief Abstract interface for a multi-producer, single-consumer ring buffer.
 *
 * Provides non-blocking and blocking push operations, a non-blocking pop
 * operation, and graceful shutdown support. Producers call push/try_push
 * from any thread; a single consumer calls try_pop.
 */
class IRing {
public:
    virtual ~IRing() = default;
    IRing(const IRing&) = delete;
    IRing& operator=(const IRing&) = delete;
    IRing(IRing&&) = delete;
    IRing& operator=(IRing&&) = delete;

    /**
     * @brief Attempt to push an entry without blocking.
     *
     * @param entry The log entry to enqueue.
     * @return Success, or RingError::FULL if the ring is at capacity,
     *         or RingError::SHUTDOWN if the ring has been shut down.
     */
    [[nodiscard]] virtual std::expected<void, RingError> try_push(const LogEntry& entry) noexcept = 0;

    /**
     * @brief Attempt to pop an entry from the ring.
     *
     * Must only be called from the single consumer thread.
     *
     * @return The popped LogEntry, or std::nullopt if the ring is empty.
     */
    [[nodiscard]] virtual std::optional<LogEntry> try_pop() noexcept = 0;

    /**
     * @brief Push an entry, blocking until space is available.
     *
     * Blocks the calling thread when the ring is full. Returns immediately
     * if the ring has been shut down.
     *
     * @param entry The log entry to enqueue.
     * @return Success, or RingError::SHUTDOWN if the ring has been shut down.
     */
    [[nodiscard]] virtual std::expected<void, RingError> push(const LogEntry& entry) = 0;

    /**
     * @brief Signal the ring to shut down.
     *
     * Wakes all blocked producers. After shutdown, try_push and push
     * return RingError::SHUTDOWN. try_pop continues to drain remaining
     * entries. This method is idempotent.
     */
    virtual void shutdown() noexcept = 0;

protected:
    IRing() = default;
};

} // namespace rtlog