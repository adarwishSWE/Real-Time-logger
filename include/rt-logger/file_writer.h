#pragma once

#include <rt-logger/ilog_writer.h>

#include <fstream>
#include <memory>
#include <ostream>
#include <string>

namespace rtlog {

class FileWriter : public ILogWriter {
public:
    [[nodiscard]] static std::unique_ptr<ILogWriter> create(std::string_view filepath);

    FileWriter() = default;
    ~FileWriter() override = default;
    FileWriter(const FileWriter&) = delete;
    FileWriter& operator=(const FileWriter&) = delete;
    FileWriter(FileWriter&&) = delete;
    FileWriter& operator=(FileWriter&&) = delete;

    explicit FileWriter(std::ostream& stream);

    [[nodiscard]] std::expected<void, WriteError> write(std::string_view message) noexcept override;

private:
    std::ofstream owned_stream_;
    std::ostream* stream_{nullptr};
};

} // namespace rtlog