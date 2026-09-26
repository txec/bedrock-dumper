#include "dumper/dumper.h"
#include "dumper/stages/registry.h"
#include "process/helpers/helpers.h"
#include "process/memory/memory.h"
#include "process/process.h"
#include "process/xref/xref.h"
#include <Zydis/Zydis.h>
#include <spdlog/spdlog.h>

namespace dumper::stages::client_instance {

    // Helper: Validates candidate function as GetLocalPlayer and extracts member displacement
    static auto inspect_get_local_player(uintptr_t func_cand) -> std::optional<size_t> {
        auto buf = process::Memory::read_bytes(func_cand, 0x50);
        if (buf.size() < 0x28) {
            return std::nullopt;
        }

        // Check prologue: sub rsp, 48h (48 83 EC 48)
        if (buf[0] != 0x48 || buf[1] != 0x83 || buf[2] != 0xEC || buf[3] != 0x48) {
            return std::nullopt;
        }

        ZydisDecoder decoder;
        ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

        size_t offset = 0;
        while (offset < buf.size()) {
            ZydisDecodedInstruction insn;
            ZydisDecodedOperand ops[ZYDIS_MAX_OPERAND_COUNT];

            if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, buf.data() + offset,
                                                     buf.size() - offset, &insn, ops))) {
                offset++;
                continue;
            }

            // Look for: lea rdx, [rcx + disp]
            if (insn.mnemonic == ZYDIS_MNEMONIC_LEA &&
                ops[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                ops[0].reg.value == ZYDIS_REGISTER_RDX &&
                ops[1].type == ZYDIS_OPERAND_TYPE_MEMORY &&
                ops[1].mem.base == ZYDIS_REGISTER_RCX &&
                ops[1].mem.disp.has_displacement) {
                return static_cast<size_t>(ops[1].mem.disp.value);
            }

            offset += insn.length;
        }

        return std::nullopt;
    }

    auto dump() -> bool {
        const uintptr_t base = process::g_process.get_module_base();
        if (!base) {
            spdlog::error("ClientInstance: Module base address is null");
            return false;
        }

        uintptr_t update_func_addr = 0;

        // =========================================================================
        // 1. Resolve ClientInstance::Update via "textures/ui/pointer" anchor string
        // =========================================================================
        spdlog::info("Searching for anchor string \"textures/ui/pointer\" in .rdata...");
        const auto string_matches = process::Memory::scan_string("textures/ui/pointer", ".rdata");

        if (!string_matches.empty()) {
            const uintptr_t string_addr = string_matches.front();
            spdlog::info("Found \"textures/ui/pointer\" at 0x{:X} (RVA: 0x{:X})", string_addr, string_addr - base);

            spdlog::info("Scanning .text for RIP-relative xrefs to string address...");
            const auto xrefs = process::g_xref.scan(string_addr, ".text");

            for (const auto& xref : xrefs) {
                spdlog::info("Found xref instruction at 0x{:X} (RVA: 0x{:X})", xref, xref - base);

                auto func_entry = process::helpers::find_function_start(xref);
                if (func_entry) {
                    update_func_addr = *func_entry;
                    const size_t rva = update_func_addr - base;

                    spdlog::info("ClientInstance::update found at 0x{:X} (RVA: 0x{:X})", update_func_addr, rva);
                    dumper::g_dumper.add_offset("ClientInstance", "Update", rva, "xref to textures/ui/pointer");
                    break;
                }
            }
        }

        // Fallback pattern scan for update
        if (!update_func_addr) {
            spdlog::warn("Attempting fallback pattern scan for ClientInstance::update...");
            const std::string_view fallback_pattern =
                "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC ? ? 00 00 48 8D AC 24";

            const auto pattern_matches = process::Memory::scan_pattern(fallback_pattern, ".text");
            if (!pattern_matches.empty()) {
                update_func_addr = pattern_matches.front();
                const size_t rva = update_func_addr - base;

                spdlog::info("ClientInstance::update found via pattern at 0x{:X} (RVA: 0x{:X})", update_func_addr, rva);
                dumper::g_dumper.add_offset("ClientInstance", "Update", rva, "pattern fallback");
            }
        }

        if (!update_func_addr) {
            spdlog::error("Failed to locate ClientInstance::update");
            return false;
        }

        // =========================================================================
        // 2. Resolve ClientInstance::GetLocalPlayer via VTable Navigation from Update
        // =========================================================================
        spdlog::info("Locating ClientInstance VTable via Update pointer in .rdata...");
        const auto vtable_slots = process::Memory::scan_pointer(update_func_addr, ".rdata");

        uintptr_t get_local_player_addr = 0;
        std::optional<size_t> local_player_member_offset;

        if (!vtable_slots.empty()) {
            const uintptr_t update_vtable_entry = vtable_slots.front();
            spdlog::info("Found ClientInstance VTable slot (Update) at 0x{:X}", update_vtable_entry);

            // In ClientInstance vtable: Update is Slot 24, GetLocalPlayer is Slot 31 (+7 slots = +0x38 bytes)
            constexpr int expected_slot_delta = 7;
            const uintptr_t primary_candidate_slot = update_vtable_entry + expected_slot_delta * sizeof(uintptr_t);

            if (auto func_ptr = process::Memory::read<uintptr_t>(primary_candidate_slot)) {
                if (auto disp = inspect_get_local_player(*func_ptr)) {
                    get_local_player_addr = *func_ptr;
                    local_player_member_offset = disp;
                    spdlog::info("Verified GetLocalPlayer at expected Slot 31 (delta +{}): 0x{:X}",
                                 expected_slot_delta, get_local_player_addr);
                }
            }

            // If slot moved across updates, scan nearby slots (+1 to +15 from Update)
            if (!get_local_player_addr) {
                spdlog::warn("Expected slot did not verify; scanning neighboring vtable slots...");
                for (int delta = 1; delta <= 15; ++delta) {
                    if (delta == expected_slot_delta) continue;

                    const uintptr_t cand_slot = update_vtable_entry + delta * sizeof(uintptr_t);
                    if (auto func_ptr = process::Memory::read<uintptr_t>(cand_slot)) {
                        if (auto disp = inspect_get_local_player(*func_ptr)) {
                            get_local_player_addr = *func_ptr;
                            local_player_member_offset = disp;
                            spdlog::info("Found GetLocalPlayer at dynamically adjusted slot delta +{}: 0x{:X}",
                                         delta, get_local_player_addr);
                            break;
                        }
                    }
                }
            }
        } else {
            spdlog::warn("No pointers to ClientInstance::update found in .rdata");
        }

        // =========================================================================
        // 3. Fallback Pattern Scan for ClientInstance::getLocalPlayer
        // =========================================================================
        if (!get_local_player_addr) {
            spdlog::warn("Attempting fallback pattern scan for ClientInstance::getLocalPlayer...");
            // 48 83 EC 48 48 8B 05 ?? ?? ?? ?? 48 31 E0 48 89 44 24 ?? 48 8D 91
            const std::string_view get_player_pattern =
                "48 83 EC 48 48 8B 05 ? ? ? ? 48 31 E0 48 89 44 24 ? 48 8D 91";

            const auto matches = process::Memory::scan_pattern(get_player_pattern, ".text");
            if (!matches.empty()) {
                get_local_player_addr = matches.front();
                local_player_member_offset = inspect_get_local_player(get_local_player_addr);
                spdlog::info("ClientInstance::getLocalPlayer found via pattern at 0x{:X}", get_local_player_addr);
            }
        }

        if (get_local_player_addr) {
            const size_t func_rva = get_local_player_addr - base;
            spdlog::info("ClientInstance::getLocalPlayer resolved at 0x{:X} (RVA: 0x{:X})",
                         get_local_player_addr, func_rva);
            dumper::g_dumper.add_offset("ClientInstance", "GetLocalPlayer", func_rva, "vtable slot from update");
        } else {
            spdlog::error("Failed to locate ClientInstance::getLocalPlayer");
        }

        return (update_func_addr != 0) && (get_local_player_addr != 0);
    }

} // namespace dumper::stages::client_instance

REGISTER_STAGE(client_instance)
