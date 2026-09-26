#include "memory.h"
#include <algorithm>
#include <cstring>
#include <sstream>

namespace process {

    auto Memory::read_bytes(uintptr_t address, size_t size) -> std::vector<uint8_t> {
        std::vector<uint8_t> buffer(size);
        SIZE_T bytes_read = 0;

        if (!ReadProcessMemory(g_process.get_handle(), reinterpret_cast<LPCVOID>(address),
                               buffer.data(), size, &bytes_read) ||
            bytes_read == 0) {
            return {};
        }

        if (bytes_read < size) {
            buffer.resize(bytes_read);
        }

        return buffer;
    }

    auto Memory::read_string(uintptr_t address, size_t max_length) -> std::optional<std::string> {
        auto bytes = read_bytes(address, max_length);
        if (bytes.empty()) {
            return std::nullopt;
        }

        auto null_pos = std::find(bytes.begin(), bytes.end(), '\0');
        if (null_pos == bytes.begin()) {
            return "";
        }

        return std::string(bytes.begin(), null_pos);
    }

    auto Memory::scan_string(const std::string& target, std::string_view section)
        -> std::vector<uintptr_t> {
        std::vector<uintptr_t> matches;
        if (target.empty()) {
            return matches;
        }

        auto sec = g_process.get_section(section);
        if (!sec) {
            return matches;
        }

        auto buffer = read_bytes(sec->first, sec->second);
        if (buffer.size() < target.size()) {
            return matches;
        }

        const size_t max_offset = buffer.size() - target.size();
        for (size_t offset = 0; offset <= max_offset; ++offset) {
            if (std::memcmp(buffer.data() + offset, target.data(), target.size()) == 0) {
                matches.push_back(sec->first + offset);
            }
        }

        return matches;
    }

    auto Memory::scan_pointer(uintptr_t target, std::string_view section)
        -> std::vector<uintptr_t> {
        std::vector<uintptr_t> matches;
        auto sec = g_process.get_section(section);
        if (!sec) {
            return matches;
        }

        auto buffer = read_bytes(sec->first, sec->second);
        if (buffer.size() < sizeof(uintptr_t)) {
            return matches;
        }

        const size_t max_offset = buffer.size() - sizeof(uintptr_t);
        for (size_t offset = 0; offset <= max_offset; offset += sizeof(uintptr_t)) {
            uintptr_t val = 0;
            std::memcpy(&val, buffer.data() + offset, sizeof(uintptr_t));
            if (val == target) {
                matches.push_back(sec->first + offset);
            }
        }

        return matches;
    }

    struct PatternByte {
        uint8_t value{0};
        bool is_wildcard{false};
    };

    static auto parse_ida_pattern(std::string_view pattern_str) -> std::vector<PatternByte> {
        std::vector<PatternByte> pattern;
        std::istringstream stream(std::string{pattern_str});
        std::string token;

        while (stream >> token) {
            if (token == "?" || token == "??") {
                pattern.push_back({0, true});
            } else {
                try {
                    uint8_t byte = static_cast<uint8_t>(std::stoul(token, nullptr, 16));
                    pattern.push_back({byte, false});
                } catch (...) {
                    pattern.push_back({0, true});
                }
            }
        }

        return pattern;
    }

    auto Memory::scan_pattern(std::string_view ida_pattern, std::string_view section)
        -> std::vector<uintptr_t> {
        std::vector<uintptr_t> matches;
        const auto pattern = parse_ida_pattern(ida_pattern);
        if (pattern.empty()) {
            return matches;
        }

        auto sec = g_process.get_section(section);
        if (!sec) {
            return matches;
        }

        auto buffer = read_bytes(sec->first, sec->second);
        if (buffer.size() < pattern.size()) {
            return matches;
        }

        const size_t max_offset = buffer.size() - pattern.size();
        for (size_t offset = 0; offset <= max_offset; ++offset) {
            bool found = true;
            for (size_t i = 0; i < pattern.size(); ++i) {
                if (!pattern[i].is_wildcard && buffer[offset + i] != pattern[i].value) {
                    found = false;
                    break;
                }
            }
            if (found) {
                matches.push_back(sec->first + offset);
            }
        }

        return matches;
    }

} // namespace process
