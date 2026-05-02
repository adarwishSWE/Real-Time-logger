#pragma once

#include <rt-logger/ilog_writer.h>

#include <memory>
#include <vector>

namespace rtlog {

class BroadcastWriter : public ILogWriter {
public:
    explicit BroadcastWriter(std::vector<std::unique_ptr<ILogWriter>> writers);
    ~BroadcastWriter() override = default;
    BroadcastWriter(const BroadcastWriter&) = delete;
    BroadcastWriter& operator=(const BroadcastWriter&) = delete;
    BroadcastWriter(BroadcastWriter&&) = default;
    BroadcastWriter& operator=(BroadcastWriter&&) = default;

    [[nodiscard]] std::expected<void, WriteError> write(std::string_view message) noexcept override;

private:
    std::vector<std::unique_ptr<ILogWriter>> writers_;
};

} // namespace rtlog