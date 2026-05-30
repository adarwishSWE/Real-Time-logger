#include <rt-logger/console_writer.h>

namespace rtlog {

std::expected<void, WriteError> ConsoleWriter::write(std::string_view message) noexcept {
    std::cout << message << '\n';
    if (std::cout.fail()) {
        return std::unexpected(WriteError::WRITE_FAILED);
    }

    return {};
}

} // namespace rtlog
