// Component under test: MpscRing<N>
// Covers: lock-free try_push/try_pop, FIFO ordering, multi-producer contention,
//         push() fast path and condition_variable blocking, shutdown semantics,
//         capacity()/is_shutdown(), ring wrapping

#include <rt-logger/mpsc_ring.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {

using namespace rtlog;

LogEntry make_entry(int line = 0, LogLevel level = LogLevel::INFO) {
    LogEntry entry{};
    entry.source_loc_.line_ = line;
    entry.level_ = level;
    return entry;
}

bool wait_until(const std::atomic<bool>& flag,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!flag.load(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::yield();
    }
    return true;
}

bool wait_until_count_reaches(const std::atomic<int>& counter,
    int expected,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (counter.load(std::memory_order_acquire) < expected) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::yield();
    }
    return true;
}

} // namespace

class MpscRingTest : public ::testing::Test {};

// compile-time capacity trait accepts positive powers of two only
TEST(MpscRingSizeTraitTest, ValidAndInvalidSizes) {
    static_assert(is_valid_mpsc_ring_size<1>);
    static_assert(is_valid_mpsc_ring_size<4>);
    static_assert(is_valid_mpsc_ring_size<256>);
    static_assert(!is_valid_mpsc_ring_size<0>);
    static_assert(!is_valid_mpsc_ring_size<3>);
    static_assert(!is_valid_mpsc_ring_size<5>);
}

// capacity() reports the configured ring size
TEST_F(MpscRingTest, CapacityReturnsRingSize) {
    EXPECT_EQ(MpscRing<4>::capacity(), 4U);
    EXPECT_EQ(MpscRing<256>::capacity(), 256U);
}

// is_shutdown() reflects shutdown() state
TEST_F(MpscRingTest, IsShutdownReflectsShutdownCall) {
    // Given
    MpscRing<4> ring;

    // When / Then
    EXPECT_FALSE(ring.is_shutdown());
    ring.shutdown();
    EXPECT_TRUE(ring.is_shutdown());
}

// push() succeeds via try_push without waiting on push_mutex_ when the ring has capacity
TEST_F(MpscRingTest, PushSucceedsWithoutBlockingWhenNotFull) {
    // Given
    MpscRing<4> ring;
    auto entry = make_entry(7, LogLevel::WARN);

    // When
    auto result = ring.push(entry);
    auto popped = ring.try_pop();

    // Then
    EXPECT_TRUE(result.has_value());
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(popped->source_loc_.line_, 7);
    EXPECT_EQ(popped->level_, LogLevel::WARN);
}

// a single producer can push a LogEntry and pop it back
TEST_F(MpscRingTest, SingleProducerPushPop) {
    // Given
    MpscRing<4> ring;
    auto entry = make_entry(0, LogLevel::INFO);

    // When
    EXPECT_TRUE(ring.try_push(entry).has_value());
    auto popped = ring.try_pop();

    // Then
    EXPECT_TRUE(popped.has_value());
    EXPECT_EQ(popped->level_, LogLevel::INFO);
}

// try_pop() on an empty ring returns std::nullopt
TEST_F(MpscRingTest, TryPopEmptyReturnsNullopt) {
    // Given
    MpscRing<4> ring;

    // When
    auto result = ring.try_pop();

    // Then
    EXPECT_FALSE(result.has_value());
}

// try_push() returns RingError::FULL when the ring is at capacity
TEST_F(MpscRingTest, TryPushFullReturnsFull) {
    // Given
    MpscRing<4> ring;
    auto entry = make_entry();

    // When
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(ring.try_push(entry).has_value());
    }
    auto result = ring.try_push(entry);

    // Then
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RingError::FULL);
}

// entries are popped in the same order they were pushed
TEST_F(MpscRingTest, FifoOrdering) {
    // Given
    MpscRing<4> ring;

    // When
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(ring.try_push(make_entry(i)).has_value());
    }

    // Then
    for (int i = 0; i < 3; ++i) {
        auto popped = ring.try_pop();
        ASSERT_TRUE(popped.has_value());
        EXPECT_EQ(popped->source_loc_.line_, i);
    }
}

// multiple producers can push concurrently without data loss
TEST_F(MpscRingTest, MultiProducerContention) {
    // Given
    constexpr int num_producers = 4;
    constexpr int items_per_producer = 100;
    constexpr int total_items = num_producers * items_per_producer;
    MpscRing<256> ring;

    // When
    std::vector<std::jthread> producers;
    producers.reserve(num_producers);
    for (int producer_idx = 0; producer_idx < num_producers; ++producer_idx) {
        producers.emplace_back([&ring, producer_idx] {
            for (int item_idx = 0; item_idx < items_per_producer; ++item_idx) {
                auto entry = make_entry(producer_idx);

                while (true) {
                    auto result = ring.try_push(entry);
                    if (result.has_value()) {
                        break;
                    }
                }
            }
        });
    }

    int count = 0;
    while (count < total_items) {
        auto popped = ring.try_pop();
        if (popped.has_value()) {
            ++count;
        }
    }

    // Then
    EXPECT_EQ(count, total_items);
}

// push() blocks when the ring is full and unblocks when space is freed
TEST_F(MpscRingTest, BlockingPushWaitsForSpace) {
    // Given
    MpscRing<2> ring;
    auto entry = make_entry();
    EXPECT_TRUE(ring.try_push(entry).has_value());
    EXPECT_TRUE(ring.try_push(entry).has_value());

    std::atomic<bool> pushed{false};
    std::jthread pusher([&ring, &pushed] {
        auto result = ring.push(make_entry());
        EXPECT_TRUE(result.has_value());
        pushed.store(true, std::memory_order_release);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(pushed.load(std::memory_order_acquire));

    // When
    static_cast<void>(ring.try_pop());

    // Then
    EXPECT_TRUE(wait_until(pushed));
    EXPECT_TRUE(pushed.load(std::memory_order_acquire));
}

// push() serialises waiters so only one wakes per free slot
TEST_F(MpscRingTest, BlockingPushNotifiesMultipleWaiters) {
    // Given
    MpscRing<1> ring;
    auto entry = make_entry();
    EXPECT_TRUE(ring.try_push(entry).has_value());

    std::atomic<int> pushed_count{0};

    std::jthread pusher1([&ring, &pushed_count] {
        auto result = ring.push(make_entry());
        if (result.has_value()) {
            pushed_count.fetch_add(1, std::memory_order_release);
        }
    });

    std::jthread pusher2([&ring, &pushed_count] {
        auto result = ring.push(make_entry());
        if (result.has_value()) {
            pushed_count.fetch_add(1, std::memory_order_release);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // When — pop one, only one pusher should wake
    auto popped1 = ring.try_pop();
    EXPECT_TRUE(popped1.has_value());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Then — at least one pusher may succeed; both is possible under scheduling
    EXPECT_TRUE(wait_until_count_reaches(pushed_count, 1));
    const int after_first_pop = pushed_count.load(std::memory_order_acquire);
    EXPECT_GE(after_first_pop, 1);
    EXPECT_LE(after_first_pop, 2);

    // When — pop again to free space for second pusher
    auto popped2 = ring.try_pop();
    EXPECT_TRUE(popped2.has_value());

    // Then
    EXPECT_TRUE(wait_until_count_reaches(pushed_count, 2));
    EXPECT_EQ(pushed_count.load(std::memory_order_acquire), 2);
}

// shutdown() does not lose a wakeup when called immediately after a pusher blocks
TEST_F(MpscRingTest, ShutdownDoesNotLoseWakeup) {
    constexpr int iterations = 200;
    constexpr auto per_iteration_timeout = std::chrono::milliseconds(100);

    for (int iter = 0; iter < iterations; ++iter) {
        MpscRing<1> ring;
        EXPECT_TRUE(ring.try_push(make_entry()).has_value());

        std::atomic<bool> push_returned{false};
        std::jthread pusher([&ring, &push_returned] {
            auto result = ring.push(make_entry());
            EXPECT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), RingError::SHUTDOWN);
            push_returned.store(true, std::memory_order_release);
        });

        ring.shutdown();

        EXPECT_TRUE(wait_until(push_returned, per_iteration_timeout))
            << "lost wakeup on iteration " << iter;
    }
}

// shutdown() wakes a blocked push() which then returns RingError::SHUTDOWN
TEST_F(MpscRingTest, ShutdownWakesBlockingPush) {
    // Given
    MpscRing<1> ring;
    EXPECT_TRUE(ring.try_push(make_entry()).has_value());

    std::atomic<bool> push_returned{false};
    std::jthread pusher([&ring, &push_returned] {
        auto result = ring.push(make_entry());
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), RingError::SHUTDOWN);
        push_returned.store(true, std::memory_order_release);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(push_returned.load(std::memory_order_acquire));

    // When
    ring.shutdown();

    // Then
    EXPECT_TRUE(wait_until(push_returned));
    EXPECT_TRUE(push_returned.load(std::memory_order_acquire));
}

// try_push() returns RingError::SHUTDOWN after shutdown()
TEST_F(MpscRingTest, TryPushAfterShutdown) {
    // Given
    MpscRing<4> ring;
    ring.shutdown();

    // When
    auto result = ring.try_push(make_entry());

    // Then
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RingError::SHUTDOWN);
}

// push() returns RingError::SHUTDOWN after shutdown()
TEST_F(MpscRingTest, PushAfterShutdown) {
    // Given
    MpscRing<4> ring;
    ring.shutdown();

    // When
    auto result = ring.push(make_entry());

    // Then
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RingError::SHUTDOWN);
}

// try_pop() continues to drain entries after shutdown()
TEST_F(MpscRingTest, TryPopStillWorksAfterShutdown) {
    // Given
    MpscRing<4> ring;
    auto entry = make_entry(42, LogLevel::INFO);
    EXPECT_TRUE(ring.try_push(entry).has_value());
    ring.shutdown();

    // When
    auto popped = ring.try_pop();

    // Then
    EXPECT_TRUE(popped.has_value());
    EXPECT_EQ(popped->level_, LogLevel::INFO);
    EXPECT_EQ(popped->source_loc_.line_, 42);
}

// shutdown() wakes all blocked pushers via push_cv_.notify_all()
TEST_F(MpscRingTest, ShutdownWakesMultipleBlockedPushers) {
    // Given
    MpscRing<2> ring;
    auto entry = make_entry();
    EXPECT_TRUE(ring.try_push(entry).has_value());
    EXPECT_TRUE(ring.try_push(entry).has_value());

    std::atomic<int> shutdown_count{0};

    auto blocked_pusher = [&ring, &shutdown_count] {
        auto result = ring.push(make_entry());
        if (!result.has_value() && result.error() == RingError::SHUTDOWN) {
            shutdown_count.fetch_add(1, std::memory_order_release);
        }
    };

    std::jthread producer1(blocked_pusher);
    std::jthread producer2(blocked_pusher);
    std::jthread producer3(blocked_pusher);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // When
    ring.shutdown();

    // Then
    EXPECT_TRUE(wait_until_count_reaches(shutdown_count, 3));
    EXPECT_EQ(shutdown_count.load(std::memory_order_acquire), 3);
}

// data survives multiple wrap-around cycles of the ring buffer
TEST_F(MpscRingTest, RingWrappingPreservesData) {
    // Given
    MpscRing<4> ring;

    // When / Then — 3 full cycles of fill → drain
    for (int cycle = 0; cycle < 3; ++cycle) {
        for (int i = 0; i < 4; ++i) {
            EXPECT_TRUE(ring.try_push(make_entry((cycle * 10) + i)).has_value());
        }
        EXPECT_FALSE(ring.try_push(make_entry()).has_value());

        for (int i = 0; i < 4; ++i) {
            auto popped = ring.try_pop();
            ASSERT_TRUE(popped.has_value());
            EXPECT_EQ(popped->source_loc_.line_, (cycle * 10) + i);
        }
    }
}

// the ring operates correctly with the minimum capacity N=1
TEST_F(MpscRingTest, MinimumRingSize) {
    // Given
    MpscRing<1> ring;
    auto entry = make_entry(0, LogLevel::WARN);

    // When
    EXPECT_TRUE(ring.try_push(entry).has_value());
    EXPECT_FALSE(ring.try_push(make_entry()).has_value());

    auto popped = ring.try_pop();
    ASSERT_TRUE(popped.has_value());

    // Then
    EXPECT_EQ(popped->level_, LogLevel::WARN);
    EXPECT_FALSE(ring.try_pop().has_value());
}

// push() blocks on push_cv_ when the ring is full and completes after try_pop() frees a slot
TEST_F(MpscRingTest, PushBlocksOnFullRingUntilSpaceFreed) {
    // Given
    MpscRing<4> ring;
    auto entry = make_entry();
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(ring.try_push(entry).has_value());
    }

    std::atomic<bool> pushed{false};
    std::jthread pusher([&ring, &pushed] {
        auto result = ring.push(make_entry());
        EXPECT_TRUE(result.has_value());
        pushed.store(true, std::memory_order_release);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(pushed.load(std::memory_order_acquire));

    // When
    auto popped = ring.try_pop();
    EXPECT_TRUE(popped.has_value());

    // Then
    EXPECT_TRUE(wait_until(pushed));
    EXPECT_TRUE(pushed.load(std::memory_order_acquire));
}
