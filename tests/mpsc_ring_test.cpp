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

TEST(MpscRing, SingleProducerPushPop) {
    MpscRing<4> ring;
    auto entry = make_entry(0, LogLevel::INFO);

    EXPECT_TRUE(ring.try_push(entry).has_value());

    auto popped = ring.try_pop();
    EXPECT_TRUE(popped.has_value());
    EXPECT_EQ(popped->level, LogLevel::INFO);
}

TEST(MpscRing, TryPopEmptyReturnsNullopt) {
    MpscRing<4> ring;
    EXPECT_FALSE(ring.try_pop().has_value());
}

TEST(MpscRing, TryPushFullReturnsFull) {
    MpscRing<4> ring;
    auto entry = make_entry();

    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(ring.try_push(entry).has_value());
    }

    auto result = ring.try_push(entry);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RingError::FULL);
}

TEST(MpscRing, FifoOrdering) {
    MpscRing<4> ring;

    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(ring.try_push(make_entry(i)).has_value());
    }

    for (int i = 0; i < 3; ++i) {
        auto popped = ring.try_pop();
        ASSERT_TRUE(popped.has_value());
        EXPECT_EQ(popped->source_loc.line, i);
    }
}

TEST(MpscRing, MultiProducerContention) {
    constexpr int kNumProducers = 4;
    constexpr int kItemsPerProducer = 100;
    constexpr int kTotalItems = kNumProducers * kItemsPerProducer;

    MpscRing<256> ring;

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

    EXPECT_EQ(count, kTotalItems);
}

TEST(MpscRing, BlockingPushWaitsForSpace) {
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

    // Give pusher time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(pushed.load(std::memory_order_acquire));

    auto __ = ring.try_pop();
    (void)__;

    // Wait for pusher to complete
    while (!pushed.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(pushed.load(std::memory_order_acquire));
}

TEST(MpscRing, BlockingPushNotifiesMultipleWaiters) {
    MpscRing<1> ring;
    auto entry = make_entry();

    EXPECT_TRUE(ring.try_push(entry).has_value());

    std::atomic<int> pushed_count{0};

    std::jthread pusher1([&ring, &pushed_count] {
        auto result = ring.push(make_entry());
        if (result.has_value()) {
            pushed_count.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::jthread pusher2([&ring, &pushed_count] {
        auto result = ring.push(make_entry());
        if (result.has_value()) {
            pushed_count.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Pop one — only one pusher should wake
    auto popped1 = ring.try_pop();
    EXPECT_TRUE(popped1.has_value());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Wait for first pusher
    while (pushed_count.load(std::memory_order_acquire) < 1) {
        std::this_thread::yield();
    }
    EXPECT_EQ(pushed_count.load(std::memory_order_acquire), 1);

    // Now pop again to free space for second pusher
    auto popped2 = ring.try_pop();
    EXPECT_TRUE(popped2.has_value());

    while (pushed_count.load(std::memory_order_acquire) < 2) {
        std::this_thread::yield();
    }
    EXPECT_EQ(pushed_count.load(std::memory_order_acquire), 2);
}

TEST(MpscRing, ShutdownWakesblockingPush) {
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

    ring.shutdown();

    while (!push_returned.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(push_returned.load(std::memory_order_acquire));
}

TEST(MpscRing, TryPushAfterShutdown) {
    MpscRing<4> ring;
    ring.shutdown();

    auto result = ring.try_push(make_entry());
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RingError::SHUTDOWN);
}

TEST(MpscRing, PushAfterShutdown) {
    MpscRing<4> ring;
    ring.shutdown();

    auto result = ring.push(make_entry());
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RingError::SHUTDOWN);
}

TEST(MpscRing, TryPopStillWorksAfterShutdown) {
    MpscRing<4> ring;
    auto entry = make_entry(42, LogLevel::INFO);

    EXPECT_TRUE(ring.try_push(entry).has_value());

    ring.shutdown();

    auto popped = ring.try_pop();
    EXPECT_TRUE(popped.has_value());
    EXPECT_EQ(popped->level, LogLevel::INFO);
    EXPECT_EQ(popped->source_loc.line, 42);
}

TEST(MpscRing, ShutdownWakesMultipleBlockedPushers) {
    MpscRing<2> ring;
    auto entry = make_entry();
    EXPECT_TRUE(ring.try_push(entry).has_value());
    EXPECT_TRUE(ring.try_push(entry).has_value());

    std::atomic<int> shutdown_count{0};

    auto blocked_pusher = [&ring, &shutdown_count] {
        auto result = ring.push(make_entry());
        if (!result.has_value() && result.error() == RingError::SHUTDOWN) {
            shutdown_count.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::jthread p1(blocked_pusher);
    std::jthread p2(blocked_pusher);
    std::jthread p3(blocked_pusher);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ring.shutdown();

    while (shutdown_count.load(std::memory_order_acquire) < 3) {
        std::this_thread::yield();
    }
    EXPECT_EQ(shutdown_count.load(std::memory_order_acquire), 3);
}

TEST(MpscRing, RingWrappingPreservesData) {
    MpscRing<4> ring;

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

TEST(MpscRing, MinimumRingSize) {
    MpscRing<1> ring;
    auto entry = make_entry(0, LogLevel::WARN);

    EXPECT_TRUE(ring.try_push(entry).has_value());
    EXPECT_FALSE(ring.try_push(make_entry()).has_value());

    auto popped = ring.try_pop();
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(popped->level, LogLevel::WARN);

    EXPECT_FALSE(ring.try_pop().has_value());
}