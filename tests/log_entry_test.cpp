#include <rt-logger/log_entry.h>

#include <gtest/gtest.h>
#include <chrono>

using namespace rtlog;

TEST(LogEntry, DefaultConstruction) {
    LogEntry entry{};
    EXPECT_EQ(entry.level, LogLevel::TRACE);
    EXPECT_EQ(entry.message[0], '\0');
}

TEST(LogEntry, AggregateInitialization) {
    auto now = std::chrono::system_clock::now();
    LogEntry entry{now, LogLevel::ERROR, {"main.cpp", 42, "foo"}, {}};
    EXPECT_EQ(entry.timestamp, now);
    EXPECT_EQ(entry.level, LogLevel::ERROR);
    EXPECT_STREQ(entry.source_loc.file, "main.cpp");
    EXPECT_EQ(entry.source_loc.line, 42);
    EXPECT_STREQ(entry.source_loc.function, "foo");
}

TEST(SourceLoc, Fields) {
    SourceLoc loc{"test.cpp", 10, "bar"};
    EXPECT_STREQ(loc.file, "test.cpp");
    EXPECT_EQ(loc.line, 10);
    EXPECT_STREQ(loc.function, "bar");
}