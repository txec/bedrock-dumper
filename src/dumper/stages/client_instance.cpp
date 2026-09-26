#include "dumper/dumper.h"
#include "dumper/stages/registry.h"
#include "process/helpers/helpers.h"
#include "process/memory/memory.h"
#include "process/process.h"
#include "process/xref/xref.h"
#include <spdlog/spdlog.h>

namespace dumper::stages::client_instance {

    auto dump() -> bool {
        const uintptr_t base = process::g_process.get_module_base();
        if (!base) {
            spdlog::error("ClientInstance: Module base address is null");
            return false;
        }

        // Method 1: String Xref resolution (textures/ui/pointer)
        spdlog::info("Searching for anchor string \"textures/ui/pointer\" in .rdata...");
        const auto string_matches = process::Memory::scan_string("textures/ui/pointer", ".rdata");

        if (!string_matches.empty()) {
            const uintptr_t string_addr = string_matches.front();
            spdlog::info("Found \"textures/ui/pointer\" at 0x{:X} (RVA: 0x{:X})", string_addr, string_addr - base);

            spdlog::info("Scanning .text for RIP-relative xrefs to string address...");
            const auto xrefs = process::g_xref.scan(string_addr, ".text");

            if (!xrefs.empty()) {
                for (const auto& xref : xrefs) {
                    spdlog::info("Found xref instruction at 0x{:X} (RVA: 0x{:X})", xref, xref - base);

                    // Resolve function entry via .pdata (runtime exception table) or backwards scan
                    auto func_entry = process::helpers::find_function_start(xref);
                    if (func_entry) {
                        const uintptr_t func_addr = *func_entry;
                        const size_t rva = func_addr - base;

                        spdlog::info("ClientInstance::update found at 0x{:X} (RVA: 0x{:X})", func_addr, rva);
                        dumper::g_dumper.add_offset("ClientInstance", "Update", rva, "xref to textures/ui/pointer");
                        return true;
                    } else {
                        spdlog::warn("Could not determine function start from xref at 0x{:X}", xref);
                    }
                }
            } else {
                spdlog::warn("No xrefs found to \"textures/ui/pointer\" in .text");
            }
        } else {
            spdlog::warn("Anchor string \"textures/ui/pointer\" not found in .rdata");
        }

        // Method 2: Fallback Pattern Scan (flexible AOB pattern)
        spdlog::warn("Running fallback pattern scan for ClientInstance::update...");
        // 55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC ? ? 00 00 48 8D AC 24
        const std::string_view fallback_pattern =
            "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC ? ? 00 00 48 8D AC 24";

        const auto pattern_matches = process::Memory::scan_pattern(fallback_pattern, ".text");
        if (!pattern_matches.empty()) {
            const uintptr_t func_addr = pattern_matches.front();
            const size_t rva = func_addr - base;

            spdlog::info("ClientInstance::update found via pattern at 0x{:X} (RVA: 0x{:X})", func_addr, rva);
            dumper::g_dumper.add_offset("ClientInstance", "Update", rva, "pattern fallback");
            return true;
        }

        spdlog::error("ClientInstance::update could not be located by xrefs or pattern");
        return false;
    }

} // namespace dumper::stages::client_instance

REGISTER_STAGE(client_instance)
