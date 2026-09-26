#include "dumper.h"
#include "stages/registry.h"
#include <spdlog/spdlog.h>

namespace dumper {

    auto Dumper::start() -> bool {
        spdlog::info("Dumper engine starting ({} stage(s) registered)...", stages::g_stage_registry.size());

        size_t successful_stages = 0;
        for (const auto& stage : stages::g_stage_registry) {
            spdlog::info("--> Running stage: {}", stage.name);
            if (stage.dump()) {
                successful_stages++;
                spdlog::info("<-- Stage {} completed successfully", stage.name);
            } else {
                spdlog::error("<-- Stage {} failed", stage.name);
            }
        }

        spdlog::info("All stages finished: {}/{} successful. Total offsets dumped: {}",
                     successful_stages, stages::g_stage_registry.size(), get_total_offset_count());

        return successful_stages > 0;
    }

    auto Dumper::add_offset(const std::string& namespace_name, const std::string& offset_name,
                            size_t offset, const std::string& comment) -> void {
        std::lock_guard<std::mutex> lock(m_offset_mutex);
        m_offsets[namespace_name].push_back({offset_name, offset, comment});

        const std::string via = comment.empty() ? "" : " (" + comment + ")";
        spdlog::info("[+] {}:{} = 0x{:X}{}", namespace_name, offset_name, offset, via);
    }

    auto Dumper::get_offset(const std::string& namespace_name, const std::string& offset_name) const
        -> std::optional<size_t> {
        std::lock_guard<std::mutex> lock(m_offset_mutex);
        auto it = m_offsets.find(namespace_name);
        if (it == m_offsets.end()) {
            return std::nullopt;
        }

        for (const auto& entry : it->second) {
            if (entry.name == offset_name) {
                return entry.offset;
            }
        }

        return std::nullopt;
    }

    auto Dumper::get_total_offset_count() const -> size_t {
        std::lock_guard<std::mutex> lock(m_offset_mutex);
        size_t count = 0;
        for (const auto& [_, entries] : m_offsets) {
            count += entries.size();
        }
        return count;
    }

} // namespace dumper
