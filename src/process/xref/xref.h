#pragma once
#include <Zydis/Zydis.h>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>
#include <string_view>

namespace process {

    struct InstructionMatch {
        uintptr_t address;
        ZydisDecodedInstruction instruction;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    };

    class Xref {
      public:
        Xref();

        auto scan(uintptr_t address, std::string_view section_name = ".text") const
            -> std::vector<uintptr_t>;

        auto decode(const uint8_t* buffer, size_t length,
                    ZydisDecodedInstruction& out_instruction,
                    ZydisDecodedOperand* out_operands) const -> bool;

      private:
        ZydisDecoder m_decoder;
    };

    inline Xref g_xref;

} // namespace process
