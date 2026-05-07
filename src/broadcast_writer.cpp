#include <rt-logger/broadcast_writer.h>

namespace rtlog {

BroadcastWriter::BroadcastWriter(std::vector<std::unique_ptr<ILogWriter>> writers)
    : writers_(std::move(writers)) {}

std::expected<void, WriteError> BroadcastWriter::write(std::string_view message) noexcept {
    for (const auto& writer : writers_) {
        auto result = writer->write(message);
        if (!result.has_value()) {
            return result;
        }
    }
    return {};
}

}  // namespace rtlog
