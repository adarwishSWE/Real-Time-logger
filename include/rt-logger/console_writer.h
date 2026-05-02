#pragma once

#include <rt-logger/ilog_writer.h>

#include <iostream>

namespace rtlog {

class ConsoleWriter : public ILogWriter {
public:
    [[nodiscard]] std::expected<void, WriteError> write(std::string_view message) noexcept override;
};

} // namespace rtlog