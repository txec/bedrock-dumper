#include "xref.h"
#include "process/memory/memory.h"
#include "process/process.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace process {

    Xref::Xref() {
        ZydisDecoderInit(&m_decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    }

    auto Xref::decode(const uint8_t* buffer, size_t length,
                      ZydisDecodedInstruction& out_instruction,
                      ZydisDecodedOperand* out_operands) const -> bool {
        return ZYAN_SUCCESS(
            ZydisDecoderDecodeFull(&m_decoder, buffer, length, &out_instruction, out_operands));
    }

    auto Xref::scan(uintptr_t target_address, std::string_view section_name) const
        -> std::vector<uintptr_t> {
        std::vector<uintptr_t> xrefs;

        auto section = g_process.get_section(section_name);
        if (!section) {
            spdlog::warn("Xref::scan: section {} not found", section_name);
            return xrefs;
        }

        const uintptr_t section_start = section->first;
        const uintptr_t section_end = section_start + section->second;

        MEMORY_BASIC_INFORMATION mbi{};
        uintptr_t current = section_start;

        while (current < section_end) {
            if (VirtualQueryEx(g_process.get_handle(), reinterpret_cast<LPCVOID>(current), &mbi,
                               sizeof(mbi)) != sizeof(mbi)) {
                break;
            }

            const uintptr_t region_start = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            const uintptr_t region_end = (std::min)(region_start + mbi.RegionSize, section_end);

            if (mbi.State == MEM_COMMIT && !(mbi.Protect & PAGE_GUARD) &&
                !(mbi.Protect & PAGE_NOACCESS)) {
                auto buffer = Memory::read_bytes(region_start, region_end - region_start);

                if (!buffer.empty()) {
                    uintptr_t offset = 0;
                    while (offset < buffer.size()) {
                        ZydisDecodedInstruction instruction;
                        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

                        if (!decode(buffer.data() + offset, buffer.size() - offset, instruction,
                                    operands)) {
                            offset++;
                            continue;
                        }

                        const uintptr_t insn_addr = region_start + offset;

                        for (int i = 0; i < instruction.operand_count_visible; i++) {
                            const auto& operand = operands[i];

                            if (operand.type == ZYDIS_OPERAND_TYPE_MEMORY &&
                                operand.mem.base == ZYDIS_REGISTER_RIP &&
                                operand.mem.disp.has_displacement) {
                                uintptr_t absolute = insn_addr + instruction.length +
                                                     operand.mem.disp.value;
                                if (absolute == target_address) {
                                    xrefs.push_back(insn_addr);
                                }
                            } else if (operand.type == ZYDIS_OPERAND_TYPE_IMMEDIATE &&
                                       operand.imm.is_relative) {
                                ZyanU64 absolute = 0;
                                if (ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&instruction, &operand,
                                                                          insn_addr,
                                                                          &absolute))) {
                                    if (absolute == target_address) {
                                        xrefs.push_back(insn_addr);
                                    }
                                }
                            }
                        }

                        offset += instruction.length;
                    }
                }
            }

            current = region_end;
        }

        return xrefs;
    }

} // namespace process
