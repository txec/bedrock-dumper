#include "logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace logger {
    auto initialize() -> void {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

        auto logger = std::make_shared<spdlog::logger>("console", console_sink);
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::info);
    }
}
