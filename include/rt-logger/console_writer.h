#pragma once

#include <rt-logger/ilog_writer.h>

#include <iostream>

namespace rtlog {

class ConsoleWriter : public ILogWriter {
public:
    ConsoleWriter() = default;
    ~ConsoleWriter() override = default;
    ConsoleWriter(const ConsoleWriter&) = default;
    ConsoleWriter& operator=(const ConsoleWriter&) = default;
    ConsoleWriter(ConsoleWriter&&) = default;
    ConsoleWriter& operator=(ConsoleWriter&&) = default;

    [[nodiscard]] std::expected<void, WriteError> write(std::string_view message) noexcept override;
};

} // namespace rtlog