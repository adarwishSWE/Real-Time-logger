#pragma once

#include <rt-logger/ilog_writer.h>

#include <memory>
#include <vector>

namespace rtlog {

class BroadcastWriter : public ILogWriter {
public:
    explicit BroadcastWriter(std::vector<std::unique_ptr<ILogWriter>> writers);

    [[nodiscard]] std::expected<void, WriteError> write(std::string_view message) noexcept override;

private:
    std::vector<std::unique_ptr<ILogWriter>> writers_;
};

} // namespace rtlog