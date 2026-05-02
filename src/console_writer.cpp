#include <rt-logger/console_writer.h>

namespace rtlog {

std::expected<void, WriteError> ConsoleWriter::write(std::string_view message) noexcept {
    if (!message.data()) {
        return std::unexpected(WriteError::WRITE_FAILED);
    }

    std::cout << message << '\n';
    if (std::cout.fail()) {
        return std::unexpected(WriteError::WRITE_FAILED);
    }

    return {};
}

} // namespace rtlog