#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <string_view>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace process {

    class Process {
      public:
        Process() = default;
        ~Process();

        auto attach(std::string_view process_name = "Minecraft.Windows.exe") -> bool;
        auto get_pid() const -> DWORD { return m_pid; }
        auto get_handle() const -> HANDLE { return m_handle; }
        auto get_module_base() const -> uintptr_t { return m_module_base; }
        auto get_module_size() const -> size_t { return m_module_size; }
        auto is_attached() const -> bool { return m_attached; }

        auto get_section(std::string_view section_name) const
            -> std::optional<std::pair<uintptr_t, size_t>>;

      private:
        auto find_process_by_name(std::string_view process_name) -> std::optional<DWORD>;
        auto open_target_process(DWORD pid) -> HANDLE;
        auto cache_module_info() -> bool;

      private:
        HANDLE m_handle{};
        DWORD m_pid{};
        uintptr_t m_module_base{};
        size_t m_module_size{};
        bool m_attached{false};
        std::string m_process_name;
    };

    inline Process g_process;

} // namespace process
