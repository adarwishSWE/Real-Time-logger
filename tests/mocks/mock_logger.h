#pragma once

#include <rt-logger/ilogger.h>

#include <gmock/gmock.h>

namespace rtlog {

class MockLogger : public ILogger {
public:
    MOCK_METHOD((std::expected<void, LoggerError>),
                log,
                (LogLevel level, std::string_view message, const SourceLoc& source_loc),
                (noexcept, override));
    MOCK_METHOD(void, set_level, (LogLevel level), (noexcept, override));
    MOCK_METHOD(void, set_writer, (std::unique_ptr<ILogWriter> writer), (noexcept, override));
    MOCK_METHOD(void, shutdown, (), (noexcept, override));
};

}  // namespace rtlog