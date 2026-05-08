/**
 * @file simple_client.cpp
 * @brief Minimal single-threaded demo of rt-logger using ConsoleWriter.
 *
 * Logs a few messages at different levels, demonstrates filtering,
 * and shuts down gracefully.
 */

#include <rt-logger/console_writer.h>
#include <rt-logger/logger.h>
#include <rt-logger/mpsc_ring.h>

#include <iostream>

int main() {
    auto ring   = std::make_unique<rtlog::MpscRing<64>>();
    auto writer = std::make_unique<rtlog::ConsoleWriter>();
    rtlog::Logger logger{std::move(ring), std::move(writer), rtlog::LogLevel::INFO};

    rtlog::SourceLoc loc{"simple_client.cpp", __LINE__, __func__};

    std::cout << "--- Logging at INFO level (TRACE and DEBUG filtered) ---\n";
    logger.log(rtlog::LogLevel::TRACE, "This trace message should NOT appear", loc);
    logger.log(rtlog::LogLevel::DEBUG, "This debug message should NOT appear", loc);
    logger.log(rtlog::LogLevel::INFO, "Hello from rt-logger!", loc);
    logger.log(rtlog::LogLevel::WARN, "A warning for your attention", loc);
    logger.log(rtlog::LogLevel::ERROR, "Something went wrong", loc);

    std::cout << "\n--- Lowering level to DEBUG ---\n";
    logger.set_level(rtlog::LogLevel::DEBUG);
    logger.log(rtlog::LogLevel::DEBUG, "Now debug messages are visible", loc);
    logger.log(rtlog::LogLevel::INFO, "Info still works", loc);

    std::cout << "\n--- Shutting down ---\n";
    logger.shutdown();

    std::cout << "Done.\n";
    return 0;
}
