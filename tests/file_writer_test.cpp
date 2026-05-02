#include <rt-logger/file_writer.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace rtlog;

TEST(FileWriter, WriteToStreamSucceeds) {
    std::ostringstream oss;
    FileWriter writer(oss);

    auto result = writer.write("hello world");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(oss.str(), "hello world\n");
}

TEST(FileWriter, WriteMultipleMessages) {
    std::ostringstream oss;
    FileWriter writer(oss);

    EXPECT_TRUE(writer.write("line1").has_value());
    EXPECT_TRUE(writer.write("line2").has_value());
    EXPECT_EQ(oss.str(), "line1\nline2\n");
}

TEST(FileWriter, WriteToFailedStream) {
    std::ostringstream oss;
    oss.setstate(std::ios::failbit);

    FileWriter writer(oss);
    auto result = writer.write("message");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), WriteError::WRITE_FAILED);
}

TEST(FileWriter, CreateReturnsNullptrForBadPath) {
    auto writer = FileWriter::create("/nonexistent/path/that/does/not/exist/log.txt");
    EXPECT_EQ(writer, nullptr);
}

TEST(FileWriter, CreateAndWriteToRealFile) {
    const std::filesystem::path test_file = std::filesystem::temp_directory_path() / "rt_logger_test.txt";

    auto writer = FileWriter::create(test_file.string());
    ASSERT_NE(writer, nullptr);

    EXPECT_TRUE(writer->write("file write test").has_value());

    writer.reset();

    std::ifstream ifs(test_file);
    std::string line;
    std::getline(ifs, line);
    EXPECT_EQ(line, "file write test");

    std::filesystem::remove(test_file);
}