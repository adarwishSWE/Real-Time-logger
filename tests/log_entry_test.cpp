// Component under test: LogEntry and SourceLoc
// Covers: aggregate construction, field access, default construction

#include <rt-logger/log_entry.h>

#include <gtest/gtest.h>
#include <chrono>

using namespace rtlog;

class LogEntryTest : public ::testing::Test {};
class SourceLocTest : public ::testing::Test {};

// value-initialized LogEntry has zeroed members
TEST_F(LogEntryTest, DefaultConstruction) {
    // Given
    LogEntry entry{};

    // When / Then
    EXPECT_EQ(entry.level, LogLevel::TRACE);
    EXPECT_EQ(entry.message[0], '\0');
}

// aggregate-initialized LogEntry preserves all fields
TEST_F(LogEntryTest, AggregateInitialization) {
    // Given
    auto now = std::chrono::system_clock::now();

    // When
    LogEntry entry{now, LogLevel::ERROR, {"main.cpp", 42, "foo"}, {}};

    // Then
    EXPECT_EQ(entry.timestamp, now);
    EXPECT_EQ(entry.level, LogLevel::ERROR);
    EXPECT_STREQ(entry.source_loc.file, "main.cpp");
    EXPECT_EQ(entry.source_loc.line, 42);
    EXPECT_STREQ(entry.source_loc.function, "foo");
}

// SourceLoc fields are correctly aggregate-initialized
TEST_F(SourceLocTest, Fields) {
    // Given
    SourceLoc loc{"test.cpp", 10, "bar"};

    // When / Then
    EXPECT_STREQ(loc.file, "test.cpp");
    EXPECT_EQ(loc.line, 10);
    EXPECT_STREQ(loc.function, "bar");
}