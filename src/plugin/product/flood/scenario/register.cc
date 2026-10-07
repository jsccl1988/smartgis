// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/flood/scenario/register.h"

#include "content/public/plugin_host.h"
#include "plugin/product/flood/commands.h"
#include "plugin/product/flood/scenario/run.h"
#include "plugin/runtime/host/capability/scenario_shell.h"
#include "plugin/runtime/host/capability/scenario_command.h"
#include "plugin/runtime/host/capability/pack_ensure.h"
#include "tool/command/command.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.flood";

bool ensure_flood_pack(content::PluginHost* host) {
  return register_flood(host) && register_flood_scenario(host);
}

struct FloodHarnessOnce {
  FloodHarnessOnce() {
    register_command_pack("flood", ensure_flood_pack);
  }
} k_flood_harness_once;

}  // namespace

int scenario_flood(HarnessShell& browser) {
  return detail::run_flood(browser);
}

bool register_flood_scenario(content::PluginHost* host) {
  return contribute_scenario_command(
      host, kPluginId, "flood.scenario.showcase", "Flood Map2d showcase",
      scenario_flood, detail::bind_plugin_scenario_shell);
}

}  // namespace plugin
