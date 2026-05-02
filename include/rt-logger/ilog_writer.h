#pragma once

#include <rt-logger/errors.h>

#include <expected>
#include <string_view>

namespace rtlog {

class ILogWriter {
public:
    virtual ~ILogWriter() = default;

    [[nodiscard]] virtual std::expected<void, WriteError> write(std::string_view message) noexcept = 0;
};

} // namespace rtlog