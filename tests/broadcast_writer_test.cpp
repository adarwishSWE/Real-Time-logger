// Component under test: BroadcastWriter
// Covers: fan-out to multiple writers, first-error short-circuit propagation,
//         empty writers list, single writer

#include <rt-logger/broadcast_writer.h>

#include <mock_log_writer.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <vector>

using namespace rtlog;
using ::testing::Return;

class BroadcastWriterTest : public ::testing::Test {};

// write() fans out to every injected writer when all succeed
TEST_F(BroadcastWriterTest, FanOutToAllWriters)
{
    // Given
    auto mock1 = std::make_unique<MockLogWriter>();
    auto mock2 = std::make_unique<MockLogWriter>();
    auto mock3 = std::make_unique<MockLogWriter>();

    EXPECT_CALL(*mock1, write(std::string_view("msg")))
        .Times(1)
        .WillOnce(Return(std::expected<void, WriteError>{}));
    EXPECT_CALL(*mock2, write(std::string_view("msg")))
        .Times(1)
        .WillOnce(Return(std::expected<void, WriteError>{}));
    EXPECT_CALL(*mock3, write(std::string_view("msg")))
        .Times(1)
        .WillOnce(Return(std::expected<void, WriteError>{}));

    std::vector<std::unique_ptr<ILogWriter>> writers;
    writers.push_back(std::move(mock1));
    writers.push_back(std::move(mock2));
    writers.push_back(std::move(mock3));

    BroadcastWriter bw(std::move(writers));

    // When
    auto result = bw.write("msg");

    // Then
    EXPECT_TRUE(result.has_value());
}

// write() short-circuits and returns the first error
TEST_F(BroadcastWriterTest, ReturnsFirstError)
{
    // Given
    auto mock1 = std::make_unique<MockLogWriter>();
    auto mock2 = std::make_unique<MockLogWriter>();

    EXPECT_CALL(*mock1, write(std::string_view("msg")))
        .Times(1)
        .WillOnce(Return(std::unexpected(WriteError::WRITE_FAILED)));
    EXPECT_CALL(*mock2, write(std::string_view("msg")))
        .Times(0);

    std::vector<std::unique_ptr<ILogWriter>> writers;
    writers.push_back(std::move(mock1));
    writers.push_back(std::move(mock2));

    BroadcastWriter bw(std::move(writers));

    // When
    auto result = bw.write("msg");

    // Then
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), WriteError::WRITE_FAILED);
}

// write() continues to subsequent writers when earlier ones succeed
TEST_F(BroadcastWriterTest, SecondWriterFails)
{
    // Given
    auto mock1 = std::make_unique<MockLogWriter>();
    auto mock2 = std::make_unique<MockLogWriter>();

    EXPECT_CALL(*mock1, write(std::string_view("msg")))
        .Times(1)
        .WillOnce(Return(std::expected<void, WriteError>{}));
    EXPECT_CALL(*mock2, write(std::string_view("msg")))
        .Times(1)
        .WillOnce(Return(std::unexpected(WriteError::FILE_OPEN_FAILED)));

    std::vector<std::unique_ptr<ILogWriter>> writers;
    writers.push_back(std::move(mock1));
    writers.push_back(std::move(mock2));

    BroadcastWriter bw(std::move(writers));

    // When
    auto result = bw.write("msg");

    // Then
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), WriteError::FILE_OPEN_FAILED);
}

// an empty writers list still returns success
TEST_F(BroadcastWriterTest, EmptyWritersList)
{
    // Given
    std::vector<std::unique_ptr<ILogWriter>> writers;
    BroadcastWriter bw(std::move(writers));

    // When
    auto result = bw.write("msg");

    // Then
    EXPECT_TRUE(result.has_value());
}

// a single writer successfully receives the message
TEST_F(BroadcastWriterTest, SingleWriterSucceeds)
{
    // Given
    auto mock = std::make_unique<MockLogWriter>();

    EXPECT_CALL(*mock, write(std::string_view("msg")))
        .Times(1)
        .WillOnce(Return(std::expected<void, WriteError>{}));

    std::vector<std::unique_ptr<ILogWriter>> writers;
    writers.push_back(std::move(mock));

    BroadcastWriter bw(std::move(writers));

    // When
    auto result = bw.write("msg");

    // Then
    EXPECT_TRUE(result.has_value());
}