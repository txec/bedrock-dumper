#pragma once
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace process::helpers {

    // Resolves exact function entry using the PE .pdata (RUNTIME_FUNCTION) exception table.
    // Extremely fast (binary search, O(log N)) and 100% accurate on x64 Windows binaries.
    auto find_function_entry_from_pdata(uintptr_t addr) -> std::optional<uintptr_t>;

    // Fallback: scans backwards looking for 0xCC (INT3) padding between functions
    auto find_function_start_backwards(uintptr_t addr, size_t max_scan = 0x4000)
        -> std::optional<uintptr_t>;

    // Primary function: tries .pdata first, falls back to backwards scan
    auto find_function_start(uintptr_t addr) -> std::optional<uintptr_t>;

} // namespace process::helpers
