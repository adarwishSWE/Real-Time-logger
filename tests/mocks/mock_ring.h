#pragma once

#include <rt-logger/iring.h>

#include <gmock/gmock.h>

namespace rtlog {

class MockRing : public IRing {
public:
    MOCK_METHOD((std::expected<void, RingError>), try_push, (const LogEntry& entry), (noexcept, override));
    MOCK_METHOD(std::optional<LogEntry>, try_pop, (), (noexcept, override));
    MOCK_METHOD((std::expected<void, RingError>), push, (const LogEntry& entry), (override));
    MOCK_METHOD(void, shutdown, (), (noexcept, override));
};

} // namespace rtlog