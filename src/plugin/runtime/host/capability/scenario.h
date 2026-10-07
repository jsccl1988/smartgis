// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_CAPABILITY_SCENARIO_H_
#define PLUGIN_RUNTIME_HOST_CAPABILITY_SCENARIO_H_

#include <initializer_list>
#include <optional>
#include <string_view>

#include "plugin/runtime/host/plugin_host_export.h"

namespace content {
struct CapabilityHost;
}

namespace plugin {

// Harness scenario capability: product packages register Interact.g4
// ops so Views exec stays a thin dispatcher, and `*.scenario.*` commands
// publish their last PluginHost::execute exit code (0 = pass).

using ScenarioOpFn = bool (*)(content::CapabilityHost& host,
                              std::string_view mode);

// Mode → plugin command id. Tables stay in product interact.cc; host only
// runs the lookup + CapabilityHost::run_plugin_command.
struct ScenarioModeBinding {
  const char* mode = nullptr;
  const char* command_id = nullptr;
};

PLUGIN_HOST_EXPORT void register_scenario_op(std::string_view name,
                                             ScenarioOpFn fn,
                                             std::string_view default_mode);

// Fixed command id (mode ignored). For mine_run / world3d_run / ….
PLUGIN_HOST_EXPORT void register_scenario_command_op(
    std::string_view name, std::string_view command_id,
    std::string_view default_mode = "");

// Mode table owned by the product pack (string literals). Unknown mode → false.
PLUGIN_HOST_EXPORT void register_scenario_mode_op(
    std::string_view name, std::string_view default_mode,
    std::initializer_list<ScenarioModeBinding> modes);

PLUGIN_HOST_EXPORT bool has_scenario_op(std::string_view name);

PLUGIN_HOST_EXPORT std::optional<bool> try_exec_scenario_op(
    content::CapabilityHost& host,
    std::string_view name,
    std::string_view mode);

PLUGIN_HOST_EXPORT void set_harness_scenario_exit(int rc);
PLUGIN_HOST_EXPORT int harness_scenario_exit();

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_CAPABILITY_SCENARIO_H_
