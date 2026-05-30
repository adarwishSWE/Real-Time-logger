/**
 * @file broadcast_client.cpp
 * @brief Demonstrates BroadcastWriter fanning out to ConsoleWriter + FileWriter.
 *
 * Modest traffic (~100 messages) so terminal rendering does not dominate.
 * Shows how to write the same log stream to multiple destinations simultaneously.
 */

#include <rt-logger/broadcast_writer.h>
#include <rt-logger/console_writer.h>
#include <rt-logger/file_writer.h>
#include <rt-logger/logger.h>
#include <rt-logger/mpsc_ring.h>

#include <cstdlib>
#include <iostream>
#include <vector>

int main() {
    auto file_writer_or_err = rtlog::FileWriter::create("broadcast_client.log");
    if (!file_writer_or_err) {
        std::cerr << "Failed to create log file\n";
        return EXIT_FAILURE;
    }

    std::vector<std::unique_ptr<rtlog::ILogWriter>> writers;
    writers.push_back(std::make_unique<rtlog::ConsoleWriter>());
    writers.push_back(std::move(file_writer_or_err));

    auto broadcast = std::make_unique<rtlog::BroadcastWriter>(std::move(writers));
    auto ring = std::make_unique<rtlog::MpscRing<64>>();
    rtlog::Logger logger{std::move(ring), std::move(broadcast), rtlog::LogLevel::INFO};

    std::cout << "=== BroadcastWriter: fanning out to ConsoleWriter + FileWriter ===\n";
    std::cout << "(Level=INFO: TRACE and DEBUG will be filtered out)\n\n";

    rtlog::SourceLoc loc{"broadcast_client.cpp", __LINE__, __func__};

    logger.log(rtlog::LogLevel::TRACE, "Broadcast: this TRACE should be FILTERED", loc);
    logger.log(rtlog::LogLevel::DEBUG, "Broadcast: this DEBUG should be FILTERED", loc);
    logger.log(rtlog::LogLevel::INFO, "Broadcast: this INFO goes to console AND file", loc);
    logger.log(rtlog::LogLevel::WARN, "Broadcast: warning to all destinations", loc);
    logger.log(rtlog::LogLevel::ERROR, "Broadcast: error to all destinations", loc);

    std::cout << "\n=== Lowering level to TRACE ===\n";
    std::cout << "(Now TRACE and DEBUG will also appear)\n\n";

    logger.set_level(rtlog::LogLevel::TRACE);
    logger.log(rtlog::LogLevel::TRACE, "Broadcast: trace detail (now visible)", loc);
    logger.log(rtlog::LogLevel::DEBUG, "Broadcast: debug detail (now visible)", loc);
    logger.log(rtlog::LogLevel::INFO, "Broadcast: info detail", loc);

    std::cout << "\n=== Shutting down (drains remaining entries) ===\n";
    logger.shutdown();

    std::cout << "Done. Log also written to: broadcast_client.log\n";
    return 0;
}
