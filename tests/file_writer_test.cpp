// Component under test: FileWriter
// Covers: write via injected std::ostream, error propagation on failed stream,
//         create() factory returning nullptr for bad paths, real file integration

#include <rt-logger/file_writer.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace rtlog;

class FileWriterTest : public ::testing::Test {};

// write() appends the message and a newline to the injected stream
TEST_F(FileWriterTest, WriteToStreamSucceeds) {
    // Given
    std::ostringstream oss;
    FileWriter writer(oss);

    // When
    auto result = writer.write("hello world");

    // Then
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(oss.str(), "hello world\n");
}

// multiple writes accumulate in the stream with newline separators
TEST_F(FileWriterTest, WriteMultipleMessages) {
    // Given
    std::ostringstream oss;
    FileWriter writer(oss);

    // When
    EXPECT_TRUE(writer.write("line1").has_value());
    EXPECT_TRUE(writer.write("line2").has_value());

    // Then
    EXPECT_EQ(oss.str(), "line1\nline2\n");
}

// write() returns WriteError::WRITE_FAILED when the stream is in a failed state
TEST_F(FileWriterTest, WriteToFailedStream) {
    // Given
    std::ostringstream oss;
    oss.setstate(std::ios::failbit);
    FileWriter writer(oss);

    // When
    auto result = writer.write("message");

    // Then
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), WriteError::WRITE_FAILED);
}

// create() returns nullptr when the file path does not exist
TEST_F(FileWriterTest, CreateReturnsNullptrForBadPath) {
    // Given
    // (bad path is hardcoded)

    // When
    auto writer = FileWriter::create("/nonexistent/path/that/does/not/exist/log.txt");

    // Then
    EXPECT_EQ(writer, nullptr);
}

// create() opens a file for append and write() persists the message
TEST_F(FileWriterTest, CreateAndWriteToRealFile) {
    // Given
    const std::filesystem::path test_file =
        std::filesystem::temp_directory_path() / "rt_logger_test.txt";

    // When
    auto writer = FileWriter::create(test_file.string());
    ASSERT_NE(writer, nullptr);
    EXPECT_TRUE(writer->write("file write test").has_value());
    writer.reset();

    // Then
    std::ifstream ifs(test_file);
    std::string line;
    std::getline(ifs, line);
    EXPECT_EQ(line, "file write test");

    std::filesystem::remove(test_file);
}
