// Component under test: LogLevel
// Covers: to_string() conversion for all log levels, operator<< output

#include <rt-logger/log_level.h>

#include <gtest/gtest.h>
#include <sstream>

using namespace rtlog;

class LogLevelTest : public ::testing::Test {};

// to_string() returns the correct string for every log level
TEST_F(LogLevelTest, ToStringAllLevels) {
    // Given
    // (enums are compile-time constants)

    // When / Then
    EXPECT_EQ(to_string(LogLevel::TRACE), "TRACE");
    EXPECT_EQ(to_string(LogLevel::DEBUG), "DEBUG");
    EXPECT_EQ(to_string(LogLevel::INFO), "INFO");
    EXPECT_EQ(to_string(LogLevel::WARN), "WARN");
    EXPECT_EQ(to_string(LogLevel::ERROR), "ERROR");
    EXPECT_EQ(to_string(LogLevel::FATAL), "FATAL");
}

// operator<< writes the level name to an output stream
TEST_F(LogLevelTest, OstreamOperator) {
    // Given
    std::ostringstream oss;

    // When
    oss << LogLevel::WARN;

    // Then
    EXPECT_EQ(oss.str(), "WARN");
}

// to_string() returns "UNKNOWN" for an out-of-range cast value
TEST_F(LogLevelTest, InvalidLevelReturnsUnknown) {
    // Given
    auto invalid = static_cast<LogLevel>(999);

    // When
    auto result = to_string(invalid);

    // Then
    EXPECT_EQ(result, "UNKNOWN");
}