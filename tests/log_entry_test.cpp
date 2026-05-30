// Component under test: LogEntry and SourceLoc
// Covers: default construction, aggregate initialization, field access

#include <rt-logger/log_entry.h>

#include <gtest/gtest.h>

using namespace rtlog;

class LogEntryTest : public ::testing::Test {};
class SourceLocTest : public ::testing::Test {};

// Default-constructed LogEntry has zero-initialized fields
TEST_F(LogEntryTest, DefaultConstruction) {
    // Given / When
    LogEntry entry{};

    // Then
    EXPECT_EQ(entry.level_, LogLevel{});
    EXPECT_EQ(entry.message_[0], '\0');
}

// LogEntry supports aggregate initialization of all fields
TEST_F(LogEntryTest, AggregateInitialization) {
    // Given
    const auto now = std::chrono::system_clock::now();

    // When
    LogEntry entry{.timestamp_ = now,
        .level_ = LogLevel::ERROR,
        .source_loc_ = {.file_ = "main.cpp", .line_ = 42, .function_ = "foo"}};

    // Then
    EXPECT_EQ(entry.timestamp_, now);
    EXPECT_EQ(entry.level_, LogLevel::ERROR);
    EXPECT_STREQ(entry.source_loc_.file_, "main.cpp");
    EXPECT_EQ(entry.source_loc_.line_, 42);
    EXPECT_STREQ(entry.source_loc_.function_, "foo");
}

// SourceLoc fields are correctly aggregate-initialized
TEST_F(SourceLocTest, Fields) {
    // Given / When
    SourceLoc loc{.file_ = "test.cpp", .line_ = 10, .function_ = "bar"};

    // Then
    EXPECT_STREQ(loc.file_, "test.cpp");
    EXPECT_EQ(loc.line_, 10);
    EXPECT_STREQ(loc.function_, "bar");
}
