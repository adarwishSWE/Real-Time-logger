// Component under test: MpscRing<N>
// Covers: lock-free try_push/try_pop, FIFO ordering, multi-producer contention,
//         blocking push via condition_variable, shutdown semantics, ring wrapping

#include <rt-logger/mpsc_ring.h>

#include <gtest/gtest.h>

#include <thread>
#include <vector>

using namespace rtlog;

LogEntry make_entry(int line = 0, LogLevel level = LogLevel::INFO) {
    LogEntry e{};
    e.source_loc.line = line;
    e.level = level;
    return e;
}

class MpscRingTest : public ::testing::Test {};

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
    EXPECT_EQ(popped->level, LogLevel::INFO);
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
        EXPECT_EQ(popped->source_loc.line, i);
    }
}

// multiple producers can push concurrently without data loss
TEST_F(MpscRingTest, MultiProducerContention) {
    // Given
    constexpr int kNumProducers = 4;
    constexpr int kItemsPerProducer = 100;
    constexpr int kTotalItems = kNumProducers * kItemsPerProducer;
    MpscRing<256> ring;

    // When
    std::vector<std::jthread> producers;
    for (int t = 0; t < kNumProducers; ++t) {
        producers.emplace_back([&ring, t] {
            for (int i = 0; i < kItemsPerProducer; ++i) {
                auto entry = make_entry(t);
                entry.source_loc.file = reinterpret_cast<const char*>(static_cast<uintptr_t>(i));

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
    while (count < kTotalItems) {
        auto popped = ring.try_pop();
        if (popped.has_value()) {
            ++count;
        }
    }

    // Then
    EXPECT_EQ(count, kTotalItems);
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
    auto __ = ring.try_pop();
    (void)__;

    // Then
    while (!pushed.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
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

    // Then
    while (pushed_count.load(std::memory_order_acquire) < 1) {
        std::this_thread::yield();
    }
    EXPECT_EQ(pushed_count.load(std::memory_order_acquire), 1);

    // When — pop again to free space for second pusher
    auto popped2 = ring.try_pop();
    EXPECT_TRUE(popped2.has_value());

    // Then
    while (pushed_count.load(std::memory_order_acquire) < 2) {
        std::this_thread::yield();
    }
    EXPECT_EQ(pushed_count.load(std::memory_order_acquire), 2);
}

// shutdown() wakes a blocked push() which then returns RingError::SHUTDOWN
TEST_F(MpscRingTest, ShutdownWakesblockingPush) {
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
    while (!push_returned.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
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
    EXPECT_EQ(popped->level, LogLevel::INFO);
    EXPECT_EQ(popped->source_loc.line, 42);
}

// shutdown() notifies all blocked pushers via notify_all()
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

    std::jthread p1(blocked_pusher);
    std::jthread p2(blocked_pusher);
    std::jthread p3(blocked_pusher);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // When
    ring.shutdown();

    // Then
    while (shutdown_count.load(std::memory_order_acquire) < 3) {
        std::this_thread::yield();
    }
    EXPECT_EQ(shutdown_count.load(std::memory_order_acquire), 3);
}

// data survives multiple wrap-around cycles of the ring buffer
TEST_F(MpscRingTest, RingWrappingPreservesData) {
    // Given
    MpscRing<4> ring;

    // When / Then — 3 full cycles of fill → drain
    for (int cycle = 0; cycle < 3; ++cycle) {
        for (int i = 0; i < 4; ++i) {
            EXPECT_TRUE(ring.try_push(make_entry(cycle * 10 + i)).has_value());
        }
        EXPECT_FALSE(ring.try_push(make_entry()).has_value());

        for (int i = 0; i < 4; ++i) {
            auto popped = ring.try_pop();
            ASSERT_TRUE(popped.has_value());
            EXPECT_EQ(popped->source_loc.line, cycle * 10 + i);
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
    EXPECT_EQ(popped->level, LogLevel::WARN);
    EXPECT_FALSE(ring.try_pop().has_value());
}

// push() blocks on a large ring until space is freed
TEST_F(MpscRingTest, PushBlocksOnSize256) {
    // Given
    MpscRing<256> ring;
    auto entry = make_entry();
    for (int i = 0; i < 256; ++i) {
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
    while (!pushed.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(pushed.load(std::memory_order_acquire));

    ring.shutdown();
}

// push() blocks on a medium ring until space is freed
TEST_F(MpscRingTest, PushBlocksOnSize64) {
    // Given
    MpscRing<64> ring;
    auto entry = make_entry();
    for (int i = 0; i < 64; ++i) {
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
    while (!pushed.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(pushed.load(std::memory_order_acquire));
}

// push() blocks on a small ring until space is freed
TEST_F(MpscRingTest, PushBlocksOnSize4) {
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
    while (!pushed.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(pushed.load(std::memory_order_acquire));
}
