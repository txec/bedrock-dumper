#include "config.h"
#include "dumper/dumper.h"
#include "logger/logger.h"
#include "process/process.h"
#include "writer/writer.h"
#include <chrono>
#include <format>
#include <iostream>
#include <spdlog/spdlog.h>

auto main() -> int {
    logger::initialize();

    spdlog::info("==================================================");
    spdlog::info("  {} v{}", PROJECT_NAME, PROJECT_VERSION);
    spdlog::info("  Dynamic Memory & Function Offset Dumper");
    spdlog::info("==================================================");

    const std::string_view target_process = "Minecraft.Windows.exe";
    spdlog::info("Attaching to {}...", target_process);

    if (!process::g_process.attach(target_process)) {
        spdlog::error("Could not find or attach to {}.", target_process);
        spdlog::error("Please ensure Minecraft Bedrock is open and running, then try again.");
        return 1;
    }

    spdlog::info("Successfully attached! PID: {}", process::g_process.get_pid());
    spdlog::info("Module Base Address: 0x{:X}", process::g_process.get_module_base());
    spdlog::info("Module Size: {:.2f} MB", process::g_process.get_module_size() / (1024.0 * 1024.0));

    // Inspect critical sections
    if (auto text_sec = process::g_process.get_section(".text")) {
        spdlog::info("Section .text:  Start = 0x{:X}, Size = 0x{:X}", text_sec->first, text_sec->second);
    }
    if (auto rdata_sec = process::g_process.get_section(".rdata")) {
        spdlog::info("Section .rdata: Start = 0x{:X}, Size = 0x{:X}", rdata_sec->first, rdata_sec->second);
    }
    if (auto pdata_sec = process::g_process.get_section(".pdata")) {
        spdlog::info("Section .pdata: Start = 0x{:X}, Size = 0x{:X}", pdata_sec->first, pdata_sec->second);
    }

    spdlog::info("Starting dumper analysis pipeline...");
    const auto start_time = std::chrono::steady_clock::now();

    const bool success = dumper::g_dumper.start();

    const auto end_time = std::chrono::steady_clock::now();
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    spdlog::info("Pipeline finished in {} ms ({:.2f} seconds)", elapsed.count(), elapsed.count() / 1000.0);

    if (success) {
        spdlog::info("Writing output files...");
        dumper::writer::g_header_writer.write("offsets", elapsed);
        dumper::writer::g_json_writer.write("offsets", elapsed);
        spdlog::info("All tasks completed successfully!");
    } else {
        spdlog::warn("No offsets were dumped. Check above logs for details.");
    }

    return success ? 0 : 1;
}
