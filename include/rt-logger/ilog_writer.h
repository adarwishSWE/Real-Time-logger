#pragma once

#include <rt-logger/errors.h>

#include <expected>
#include <string_view>

namespace rtlog {

class ILogWriter {
public:
    virtual ~ILogWriter() = default;
    ILogWriter(const ILogWriter&) = delete;
    ILogWriter& operator=(const ILogWriter&) = delete;
    ILogWriter(ILogWriter&&) = delete;
    ILogWriter& operator=(ILogWriter&&) = delete;

    [[nodiscard]] virtual std::expected<void, WriteError> write(std::string_view message) noexcept = 0;

protected:
    ILogWriter() = default;
};

} // namespace rtlog