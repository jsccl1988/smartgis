// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/stormsurge/scenario/register.h"

#include "content/public/plugin_host.h"
#include "plugin/product/stormsurge/commands.h"
#include "plugin/product/stormsurge/scenario/interact.h"
#include "plugin/product/stormsurge/scenario/run.h"
#include "plugin/runtime/host/capability/scenario_shell.h"
#include "plugin/runtime/host/capability/scenario_command.h"
#include "plugin/runtime/host/capability/pack_ensure.h"
#include "tool/command/command.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.stormsurge";

bool ensure_stormsurge_pack(content::PluginHost* host) {
  (void)register_stormsurge(host);
  return register_stormsurge_scenario(host);
}

struct StormsurgeHarnessOnce {
  StormsurgeHarnessOnce() {
    register_stormsurge_interact_ops();
    register_command_pack("stormsurge", ensure_stormsurge_pack);
  }
} k_stormsurge_harness_once;

}  // namespace

int scenario_stormsurge(HarnessShell& browser) {
  return detail::run_stormsurge_scene3d(browser);
}

bool register_stormsurge_scenario(content::PluginHost* host) {
  return contribute_scenario_command(
      host, kPluginId, "stormsurge.scenario.showcase",
      "Stormsurge Scene3D showcase", scenario_stormsurge,
      detail::bind_plugin_scenario_shell);
}

}  // namespace plugin
