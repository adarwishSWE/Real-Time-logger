#pragma once

#include <rt-logger/ilog_writer.h>

#include <fstream>
#include <memory>
#include <ostream>
#include <string>

namespace rtlog {

/**
 * @brief Writes formatted log lines to a file or an injected std::ostream.
 *
 * Two construction paths:
 *   - create(filepath) — opens a real file and manages its lifetime.
 *   - FileWriter(stream) — writes to an externally-owned stream (for testing).
 */
class FileWriter : public ILogWriter {
public:
    /**
     * @brief Factory: open a file for appending and return a writer.
     *
     * @param filepath Path to the log file (opened in append mode).
     * @return Ownership of an ILogWriter, or nullptr if the file cannot be opened.
     */
    [[nodiscard]] static std::unique_ptr<ILogWriter> create(std::string_view filepath);

    FileWriter() = default;
    ~FileWriter() override = default;
    FileWriter(const FileWriter&) = delete;
    FileWriter& operator=(const FileWriter&) = delete;
    FileWriter(FileWriter&&) = delete;
    FileWriter& operator=(FileWriter&&) = delete;

    /**
     * @brief Construct with an externally-owned stream (for testing).
     *
     * The caller must ensure the stream outlives this FileWriter.
     *
     * @param stream Reference to an existing output stream.
     */
    explicit FileWriter(std::ostream& stream);

    /**
     * @brief Write a formatted log line, followed by a newline.
     *
     * Calls assert(stream_ != nullptr) in debug; calls std::terminate()
     * in release if stream_ is null (broken invariant).
     *
     * @param message The formatted log line.
     * @return Success, or WriteError::WRITE_FAILED if the stream is in a bad state.
     */
    [[nodiscard]] std::expected<void, WriteError> write(std::string_view message) noexcept override;

private:
    std::ofstream owned_stream_;
    std::ostream* stream_{nullptr};
};

} // namespace rtlog