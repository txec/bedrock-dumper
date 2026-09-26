#include "process.h"
#include "process/memory/memory.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace process {

    Process::~Process() {
        if (m_handle) {
            CloseHandle(m_handle);
            m_handle = nullptr;
        }
    }

    auto Process::attach(std::string_view process_name) -> bool {
        if (m_attached && m_handle) {
            CloseHandle(m_handle);
            m_handle = nullptr;
            m_attached = false;
            m_module_base = 0;
            m_module_size = 0;
        }

        m_process_name = process_name;
        const auto pid = find_process_by_name(process_name);
        if (!pid) {
            return false;
        }

        m_pid = *pid;
        m_handle = open_target_process(m_pid);
        if (!m_handle) {
            spdlog::error("Failed to open process PID {} with required rights", m_pid);
            return false;
        }

        if (!cache_module_info()) {
            spdlog::error("Failed to enumerate module information for PID {}", m_pid);
            CloseHandle(m_handle);
            m_handle = nullptr;
            return false;
        }

        m_attached = true;
        return true;
    }

    auto Process::find_process_by_name(std::string_view process_name) -> std::optional<DWORD> {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            return std::nullopt;
        }

        PROCESSENTRY32W pe = {sizeof(PROCESSENTRY32W)};
        std::wstring wide_name(process_name.begin(), process_name.end());

        if (Process32FirstW(snapshot, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, wide_name.c_str()) == 0) {
                    CloseHandle(snapshot);
                    return pe.th32ProcessID;
                }
            } while (Process32NextW(snapshot, &pe));
        }

        CloseHandle(snapshot);
        return std::nullopt;
    }

    auto Process::open_target_process(DWORD pid) -> HANDLE {
        // Try read & query first (best practice for UWP apps)
        HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (h) return h;

        // Fallback to all access
        h = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (h) return h;

        // Fallback to limited query + read
        h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        return h;
    }

    auto Process::cache_module_info() -> bool {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_pid);
        if (snapshot == INVALID_HANDLE_VALUE) {
            return false;
        }

        MODULEENTRY32W me = {sizeof(MODULEENTRY32W)};
        if (Module32FirstW(snapshot, &me)) {
            m_module_base = reinterpret_cast<uintptr_t>(me.modBaseAddr);
            m_module_size = me.modBaseSize;
            CloseHandle(snapshot);
            return true;
        }

        CloseHandle(snapshot);
        return false;
    }

    auto Process::get_section(std::string_view section_name) const
        -> std::optional<std::pair<uintptr_t, size_t>> {
        if (!m_module_base) {
            return std::nullopt;
        }

        auto dos_header = Memory::read<IMAGE_DOS_HEADER>(m_module_base);
        if (!dos_header || dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
            return std::nullopt;
        }

        auto nt_headers = Memory::read<IMAGE_NT_HEADERS64>(m_module_base + dos_header->e_lfanew);
        if (!nt_headers || nt_headers->Signature != IMAGE_NT_SIGNATURE) {
            return std::nullopt;
        }

        uintptr_t section_header_addr =
            m_module_base + dos_header->e_lfanew + sizeof(IMAGE_NT_HEADERS64);

        for (WORD i = 0; i < nt_headers->FileHeader.NumberOfSections; i++) {
            auto section = Memory::read<IMAGE_SECTION_HEADER>(section_header_addr +
                                                              (i * sizeof(IMAGE_SECTION_HEADER)));
            if (!section) {
                continue;
            }

            char name_buf[9] = {0};
            std::memcpy(name_buf, section->Name, 8);
            std::string name(name_buf);

            if (name == section_name) {
                uintptr_t section_start = m_module_base + section->VirtualAddress;
                size_t section_size = section->Misc.VirtualSize;
                return std::make_pair(section_start, section_size);
            }
        }

        return std::nullopt;
    }

} // namespace process
