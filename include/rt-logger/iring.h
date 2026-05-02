#pragma once

#include <rt-logger/errors.h>
#include <rt-logger/log_entry.h>

#include <expected>
#include <optional>

namespace rtlog {

class IRing {
public:
    virtual ~IRing() = default;
    IRing(const IRing&) = delete;
    IRing& operator=(const IRing&) = delete;
    IRing(IRing&&) = delete;
    IRing& operator=(IRing&&) = delete;

    [[nodiscard]] virtual std::expected<void, RingError> try_push(const LogEntry& entry) noexcept = 0;
    [[nodiscard]] virtual std::optional<LogEntry> try_pop() noexcept = 0;
    [[nodiscard]] virtual std::expected<void, RingError> push(const LogEntry& entry) = 0;
    virtual void shutdown() noexcept = 0;

protected:
    IRing() = default;
};

} // namespace rtlog