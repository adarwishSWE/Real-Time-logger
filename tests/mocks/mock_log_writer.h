#pragma once

#include <rt-logger/ilog_writer.h>

#include <gmock/gmock.h>

namespace rtlog {

class MockLogWriter : public ILogWriter {
public:
    MOCK_METHOD((std::expected<void, WriteError>), write, (std::string_view message),
        (noexcept, override));
};

} // namespace rtlog
