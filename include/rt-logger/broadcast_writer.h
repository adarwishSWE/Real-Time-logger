#pragma once

#include <rt-logger/ilog_writer.h>

#include <memory>
#include <vector>

namespace rtlog {

/**
 * @brief Fan-out writer that dispatches to multiple ILogWriter instances.
 *
 * Writes the same message to every writer in order. Returns the first
 * error encountered; subsequent writers are not invoked after a failure.
 */
class BroadcastWriter : public ILogWriter {
public:
    /**
     * @brief Construct with a list of writers. Takes ownership of all.
     *
     * @param writers Vector of writers to fan out to.
     */
    explicit BroadcastWriter(std::vector<std::unique_ptr<ILogWriter>> writers);

    ~BroadcastWriter() override                        = default;
    BroadcastWriter(const BroadcastWriter&)            = delete;
    BroadcastWriter& operator=(const BroadcastWriter&) = delete;
    BroadcastWriter(BroadcastWriter&&)                 = default;
    BroadcastWriter& operator=(BroadcastWriter&&)      = default;

    /**
     * @brief Write the message to every writer in order.
     *
     * Iterates through all writers. If any writer fails, returns that
     * error immediately and does not invoke remaining writers.
     *
     * @param message The formatted log line.
     * @return Success, or the first WriteError encountered.
     */
    [[nodiscard]] std::expected<void, WriteError> write(std::string_view message) noexcept override;

private:
    std::vector<std::unique_ptr<ILogWriter>> writers_;
};

}  // namespace rtlog
