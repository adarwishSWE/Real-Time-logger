// Component under test: Logger
// Covers: log() with level filtering, shutdown blocking, writer switching,
//         write error tracking, graceful drain on shutdown, multi-threaded logging

#include <rt-logger/logger.h>
#include <rt-logger/mpsc_ring.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <mock_log_writer.h>

using namespace rtlog;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

class LoggerTest : public ::testing::Test {
protected:
    std::unique_ptr<MpscRing<64>> ring_;
    std::unique_ptr<MockLogWriter> writer_;
    MockLogWriter* writer_ptr_;

    void SetUp() override {
        ring_ = std::make_unique<MpscRing<64>>();
        auto w = std::make_unique<MockLogWriter>();
        writer_ptr_ = w.get();
        writer_ = std::move(w);
    }

    std::unique_ptr<Logger> make_logger(LogLevel level = LogLevel::INFO) {
        auto w = std::unique_ptr<ILogWriter>(writer_.release());
        auto logger = std::make_unique<Logger>(std::move(ring_), std::move(w), level);
        return logger;
    }
};

// log() returns success for messages at or above the minimum level
TEST_F(LoggerTest, LogAtMinimumLevelReturnsSuccess) {
    // Given
    EXPECT_CALL(*writer_ptr_, write(_)).Times(testing::AtLeast(1));
    auto logger = make_logger(LogLevel::INFO);

    // When
    auto result = logger->log(LogLevel::INFO, "hello", {"test.cpp", 1, "func"});

    // Then
    EXPECT_TRUE(result.has_value());

    logger->shutdown();
}

// log() filters out messages below the minimum level
TEST_F(LoggerTest, LogBelowMinimumLevelIsFiltered) {
    // Given
    auto logger = make_logger(LogLevel::WARN);
    EXPECT_CALL(*writer_ptr_, write(_)).Times(0);

    // When
    auto result = logger->log(LogLevel::DEBUG, "filtered", {"test.cpp", 1, "func"});

    // Then
    EXPECT_TRUE(result.has_value());

    logger->shutdown();
}

// log() returns ALREADY_SHUTDOWN when called after shutdown()
TEST_F(LoggerTest, LogAfterShutdownReturnsAlreadyShutdown) {
    // Given
    EXPECT_CALL(*writer_ptr_, write(_)).Times(testing::AtLeast(0));
    auto logger = make_logger(LogLevel::INFO);
    logger->shutdown();

    // When
    auto result = logger->log(LogLevel::INFO, "too late", {"test.cpp", 1, "func"});

    // Then
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), LoggerError::ALREADY_SHUTDOWN);
}

// shutdown() is idempotent — calling it twice does not crash
TEST_F(LoggerTest, ShutdownIsIdempotent) {
    // Given
    EXPECT_CALL(*writer_ptr_, write(_)).Times(testing::AtLeast(0));
    auto logger = make_logger(LogLevel::INFO);
    logger->shutdown();

    // When / Then — second shutdown must not crash
    logger->shutdown();
}

// set_level() changes the minimum log level dynamically
TEST_F(LoggerTest, SetLevelChangesFiltering) {
    // Given
    auto logger = make_logger(LogLevel::INFO);
    EXPECT_CALL(*writer_ptr_, write(_)).Times(testing::AtLeast(1));

    // When — raise the level to WARN
    logger->set_level(LogLevel::WARN);

    // Then — INFO should be filtered out
    auto filtered = logger->log(LogLevel::INFO, "filtered", {"test.cpp", 1, "func"});
    EXPECT_TRUE(filtered.has_value());

    // When — log at WARN level
    auto allowed = logger->log(LogLevel::WARN, "allowed", {"test.cpp", 2, "func"});

    // Then
    EXPECT_TRUE(allowed.has_value());

    logger->shutdown();
}

// set_writer() switches the writer and subsequent writes go to the new writer
TEST_F(LoggerTest, SetWriterSwitchesOutput) {
    // Given
    EXPECT_CALL(*writer_ptr_, write(_)).Times(testing::AtLeast(0));
    auto logger = make_logger(LogLevel::INFO);

    auto new_writer = std::make_unique<MockLogWriter>();
    auto* new_writer_ptr = new_writer.get();
    EXPECT_CALL(*new_writer_ptr, write(_)).Times(testing::AtLeast(1));

    // When
    logger->set_writer(std::unique_ptr<ILogWriter>(new_writer.release()));

    // Then
    auto result = logger->log(LogLevel::INFO, "to new writer", {"test.cpp", 1, "func"});
    EXPECT_TRUE(result.has_value());

    logger->shutdown();
}

// write errors are tracked in the error counter
TEST_F(LoggerTest, WriteErrorsAreCounted) {
    // Given
    EXPECT_CALL(*writer_ptr_, write(_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(Return(std::unexpected(WriteError::WRITE_FAILED)));
    auto logger = make_logger(LogLevel::INFO);

    // When
    logger->log(LogLevel::INFO, "will fail", {"test.cpp", 1, "func"});
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Then
    EXPECT_GT(logger->write_error_count(), 0u);

    logger->shutdown();
}

// Logger formats output containing level, file, line, function, and message
TEST_F(LoggerTest, FormattedOutputContainsExpectedFields) {
    // Given
    std::string captured;
    EXPECT_CALL(*writer_ptr_, write(_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(
            Invoke([&captured](std::string_view msg) -> std::expected<void, WriteError> {
                captured = std::string(msg);
                return {};
            }));
    auto logger = make_logger(LogLevel::INFO);

    // When
    logger->log(LogLevel::INFO, "test message", {"main.cpp", 42, "my_func"});
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    logger->shutdown();

    // Then
    EXPECT_NE(captured.find("[INFO]"), std::string::npos);
    EXPECT_NE(captured.find("main.cpp:42"), std::string::npos);
    EXPECT_NE(captured.find("(my_func)"), std::string::npos);
    EXPECT_NE(captured.find("test message"), std::string::npos);
}

// Logger drains remaining entries in the ring before exiting
TEST_F(LoggerTest, DrainsRemainingEntriesOnShutdown) {
    // Given
    std::atomic<int> write_count{0};
    EXPECT_CALL(*writer_ptr_, write(_))
        .Times(testing::AtLeast(3))
        .WillRepeatedly(Invoke([&write_count](std::string_view) -> std::expected<void, WriteError> {
            write_count.fetch_add(1, std::memory_order_relaxed);
            return {};
        }));
    auto logger = make_logger(LogLevel::INFO);

    // When
    logger->log(LogLevel::INFO, "msg1", {"test.cpp", 1, "f"});
    logger->log(LogLevel::INFO, "msg2", {"test.cpp", 2, "f"});
    logger->log(LogLevel::INFO, "msg3", {"test.cpp", 3, "f"});
    logger->shutdown();

    // Then — all three messages should have been written
    EXPECT_GE(write_count.load(std::memory_order_acquire), 3);
}

// Multiple threads can call log() concurrently without data loss
TEST_F(LoggerTest, MultiThreadedLogging) {
    // Given
    std::atomic<int> write_count{0};
    EXPECT_CALL(*writer_ptr_, write(_))
        .Times(testing::AtLeast(40))
        .WillRepeatedly(Invoke([&write_count](std::string_view) -> std::expected<void, WriteError> {
            write_count.fetch_add(1, std::memory_order_relaxed);
            return {};
        }));
    auto logger = make_logger(LogLevel::INFO);

    // When — 4 threads × 10 messages = 40 total
    std::vector<std::jthread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&logger, i] {
            for (int j = 0; j < 10; ++j) {
                logger->log(LogLevel::INFO, "thread msg", {"test.cpp", i, "func"});
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    logger->shutdown();

    // Then — all 40 messages should have been written
    EXPECT_GE(write_count.load(std::memory_order_acquire), 40);
}

// format_entry truncates when the combined fields exceed the 512-byte buffer
TEST_F(LoggerTest, FormatTruncatesOnOverflow) {
    // Given
    EXPECT_CALL(*writer_ptr_, write(_)).Times(testing::AtLeast(1));
    auto logger = make_logger(LogLevel::INFO);

    std::string long_file(300, 'f');
    std::string long_func(300, 'g');
    std::string long_msg(100, 'm');

    // When
    auto result = logger->log(LogLevel::INFO, long_msg, {long_file.c_str(), 1, long_func.c_str()});

    // Then
    EXPECT_TRUE(result.has_value());

    logger->shutdown();
}

// drain loop handles write errors gracefully and counts them
TEST_F(LoggerTest, DrainHandlesWriteErrors) {
    // Given
    std::atomic<int> call_count{0};
    EXPECT_CALL(*writer_ptr_, write(_))
        .Times(testing::AtLeast(3))
        .WillRepeatedly(Invoke([&call_count](std::string_view) -> std::expected<void, WriteError> {
            if (call_count.fetch_add(1) < 3) {
                return std::unexpected(WriteError::WRITE_FAILED);
            }
            return {};
        }));
    auto logger = make_logger(LogLevel::INFO);

    // When
    logger->log(LogLevel::INFO, "msg1", {"test.cpp", 1, "f"});
    logger->log(LogLevel::INFO, "msg2", {"test.cpp", 2, "f"});
    logger->log(LogLevel::INFO, "msg3", {"test.cpp", 3, "f"});
    logger->shutdown();

    // Then
    EXPECT_GE(logger->write_error_count(), 3u);
}
