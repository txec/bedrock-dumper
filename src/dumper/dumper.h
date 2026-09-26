#pragma once
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dumper {

    struct OffsetEntry {
        std::string name;
        size_t offset;
        std::string comment;
    };

    class Dumper {
      public:
        Dumper() = default;
        ~Dumper() = default;

        auto start() -> bool;

        auto add_offset(const std::string& namespace_name, const std::string& offset_name,
                        size_t offset, const std::string& comment = "") -> void;

        auto get_offset(const std::string& namespace_name, const std::string& offset_name) const
            -> std::optional<size_t>;

        auto get_total_offset_count() const -> size_t;

      public:
        std::unordered_map<std::string, std::vector<OffsetEntry>> m_offsets;
        mutable std::mutex m_offset_mutex;
    };

    inline Dumper g_dumper;

} // namespace dumper
