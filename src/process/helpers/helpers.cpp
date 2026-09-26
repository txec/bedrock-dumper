#include "helpers.h"
#include "process/process.h"
#include "process/memory/memory.h"
#include <spdlog/spdlog.h>
#include <vector>

namespace process::helpers {

    struct PeRuntimeFunction {
        uint32_t begin_rva;
        uint32_t end_rva;
        uint32_t unwind_info_rva;
    };

    auto find_function_entry_from_pdata(uintptr_t addr) -> std::optional<uintptr_t> {
        const uintptr_t base = g_process.get_module_base();
        if (!base || addr < base) {
            return std::nullopt;
        }

        const uint32_t target_rva = static_cast<uint32_t>(addr - base);

        auto pdata = g_process.get_section(".pdata");
        if (!pdata || pdata->second < sizeof(PeRuntimeFunction)) {
            return std::nullopt;
        }

        const uintptr_t pdata_start = pdata->first;
        const size_t total_entries = pdata->second / sizeof(PeRuntimeFunction);

        size_t low = 0;
        size_t high = total_entries - 1;

        while (low <= high) {
            size_t mid = low + (high - low) / 2;
            auto entry = Memory::read<PeRuntimeFunction>(pdata_start + mid * sizeof(PeRuntimeFunction));
            if (!entry) {
                break;
            }

            if (target_rva < entry->begin_rva) {
                if (mid == 0) break;
                high = mid - 1;
            } else if (target_rva >= entry->end_rva) {
                low = mid + 1;
            } else {
                // Address falls inside [begin_rva, end_rva)
                return base + entry->begin_rva;
            }
        }

        return std::nullopt;
    }

    auto find_function_start_backwards(uintptr_t addr, size_t max_scan)
        -> std::optional<uintptr_t> {
        if (addr < max_scan) return std::nullopt;

        const uintptr_t region_start = addr - max_scan;
        auto buffer = Memory::read_bytes(region_start, max_scan);
        if (buffer.empty()) {
            return std::nullopt;
        }

        for (size_t i = buffer.size() - 1; i > 1; --i) {
            if (buffer[i] == 0xCC && buffer[i - 1] == 0xCC) {
                while (i < buffer.size() && buffer[i] == 0xCC) {
                    i++;
                }
                if (i < buffer.size()) {
                    return region_start + i;
                }
            }
        }

        return std::nullopt;
    }

    auto find_function_start(uintptr_t addr) -> std::optional<uintptr_t> {
        if (auto pdata_res = find_function_entry_from_pdata(addr)) {
            return pdata_res;
        }

        return find_function_start_backwards(addr);
    }

} // namespace process::helpers
