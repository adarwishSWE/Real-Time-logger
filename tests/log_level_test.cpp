#include <rt-logger/log_level.h>

#include <gtest/gtest.h>
#include <sstream>

using namespace rtlog;

TEST(LogLevel, ToStringAllLevels) {
    EXPECT_EQ(to_string(LogLevel::TRACE), "TRACE");
    EXPECT_EQ(to_string(LogLevel::DEBUG), "DEBUG");
    EXPECT_EQ(to_string(LogLevel::INFO), "INFO");
    EXPECT_EQ(to_string(LogLevel::WARN), "WARN");
    EXPECT_EQ(to_string(LogLevel::ERROR), "ERROR");
    EXPECT_EQ(to_string(LogLevel::FATAL), "FATAL");
}

TEST(LogLevel, OstreamOperator) {
    std::ostringstream oss;
    oss << LogLevel::WARN;
    EXPECT_EQ(oss.str(), "WARN");
}