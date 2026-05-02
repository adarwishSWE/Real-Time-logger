#include <rt-logger/file_writer.h>

#include <cassert>

namespace rtlog {

std::unique_ptr<ILogWriter> FileWriter::create(std::string_view filepath) {
    auto writer = std::make_unique<FileWriter>();
    writer->owned_stream_.open(std::string(filepath), std::ios::app);
    if (!writer->owned_stream_.is_open()) {
        return nullptr;
    }
    writer->stream_ = &writer->owned_stream_;
    return writer;
}

FileWriter::FileWriter(std::ostream& stream)
    : stream_(&stream) {}

std::expected<void, WriteError> FileWriter::write(std::string_view message) noexcept {
    assert(stream_ != nullptr);
    *stream_ << message << '\n';
    if (stream_->fail()) {
        return std::unexpected(WriteError::WRITE_FAILED);
    }

    stream_->flush();
    return {};
}

} // namespace rtlog