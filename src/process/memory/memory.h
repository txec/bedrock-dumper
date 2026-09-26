#pragma once
#include "process/process.h"
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace process {

    class Memory {
      public:
        template <typename T>
        static auto read(uintptr_t address) -> std::optional<T> {
            T buffer{};
            SIZE_T bytes_read = 0;

            if (!ReadProcessMemory(g_process.get_handle(), reinterpret_cast<LPCVOID>(address),
                                   &buffer, sizeof(T), &bytes_read) ||
                bytes_read != sizeof(T)) {
                return std::nullopt;
            }

            return buffer;
        }

        static auto read_bytes(uintptr_t address, size_t size) -> std::vector<uint8_t>;
        static auto read_string(uintptr_t address, size_t max_length = 256) -> std::optional<std::string>;
        static auto scan_string(const std::string& target, std::string_view section = ".rdata")
            -> std::vector<uintptr_t>;
        static auto scan_pointer(uintptr_t target, std::string_view section = ".rdata")
            -> std::vector<uintptr_t>;
        static auto scan_pattern(std::string_view ida_pattern, std::string_view section = ".text")
            -> std::vector<uintptr_t>;
    };

} // namespace process
