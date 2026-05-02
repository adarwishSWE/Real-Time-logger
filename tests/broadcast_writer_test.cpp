#include <rt-logger/broadcast_writer.h>

#include <mock_log_writer.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <vector>

using namespace rtlog;
using ::testing::Return;

TEST(BroadcastWriter, FanOutToAllWriters) {
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
    auto result = bw.write("msg");
    EXPECT_TRUE(result.has_value());
}

TEST(BroadcastWriter, ReturnsFirstError) {
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
    auto result = bw.write("msg");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), WriteError::WRITE_FAILED);
}

TEST(BroadcastWriter, SecondWriterFails) {
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
    auto result = bw.write("msg");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), WriteError::FILE_OPEN_FAILED);
}

TEST(BroadcastWriter, EmptyWritersList) {
    std::vector<std::unique_ptr<ILogWriter>> writers;
    BroadcastWriter bw(std::move(writers));

    auto result = bw.write("msg");
    EXPECT_TRUE(result.has_value());
}

TEST(BroadcastWriter, SingleWriterSucceeds) {
    auto mock = std::make_unique<MockLogWriter>();

    EXPECT_CALL(*mock, write(std::string_view("msg")))
        .Times(1)
        .WillOnce(Return(std::expected<void, WriteError>{}));

    std::vector<std::unique_ptr<ILogWriter>> writers;
    writers.push_back(std::move(mock));

    BroadcastWriter bw(std::move(writers));
    auto result = bw.write("msg");
    EXPECT_TRUE(result.has_value());
}