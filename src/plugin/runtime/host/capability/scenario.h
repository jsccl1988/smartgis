// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_CAPABILITY_SCENARIO_H_
#define PLUGIN_RUNTIME_HOST_CAPABILITY_SCENARIO_H_

#include <optional>
#include <string_view>

#include "plugin/runtime/host/plugin_host_export.h"

namespace content {
struct CapabilityHost;
}

namespace plugin {

// Harness scenario capability: product packages register Interact.g4
// verbs so Views exec stays a thin dispatcher, and `*.scenario.*` commands
// publish their last PluginHost::execute exit code (0 = pass).

using ScenarioVerbFn = bool (*)(content::CapabilityHost& host,
                                std::string_view mode);

PLUGIN_HOST_EXPORT void register_scenario_verb(std::string_view name,
                                               ScenarioVerbFn fn,
                                               std::string_view default_mode);

PLUGIN_HOST_EXPORT bool has_scenario_verb(std::string_view name);

PLUGIN_HOST_EXPORT std::optional<bool> try_exec_scenario_verb(
    content::CapabilityHost& host,
    std::string_view name,
    std::string_view mode);

PLUGIN_HOST_EXPORT void set_harness_scenario_exit(int rc);
PLUGIN_HOST_EXPORT int harness_scenario_exit();

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_CAPABILITY_SCENARIO_H_
