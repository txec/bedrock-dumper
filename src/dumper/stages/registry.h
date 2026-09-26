#pragma once
#include <functional>
#include <string>
#include <vector>

namespace dumper::stages {

    struct StageInfo {
        std::string name;
        std::function<bool()> dump;
    };

    inline std::vector<StageInfo> g_stage_registry;

    struct AutoStage {
        AutoStage(std::string name, std::function<bool()> dump) {
            g_stage_registry.push_back({std::move(name), std::move(dump)});
        }
    };

} // namespace dumper::stages

#define REGISTER_STAGE(ns)                                                               \
    namespace {                                                                          \
        const ::dumper::stages::AutoStage ns##_registrar{#ns, &::dumper::stages::ns::dump}; \
    }
