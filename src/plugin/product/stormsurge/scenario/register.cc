// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/stormsurge/scenario/register.h"

#include "content/public/plugin_host.h"
#include "plugin/product/stormsurge/scenario/interact.h"
#include "plugin/product/stormsurge/scenario/run.h"
#include "plugin/product/world3d/scenario/common/plugin_io.h"
#include "plugin/runtime/host/capability/scenario.h"
#include "plugin/runtime/host/capability/shell.h"
#include "tool/command/command.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.stormsurge";

bool run_bound(content::PluginHost* host, int (*fn)(HarnessShell&)) {
  HarnessShell* shell = harness_shell(host);
  if (!shell || !fn) {
    set_harness_scenario_exit(1);
    return false;
  }
  detail::bind_plugin_showcase_shell(shell);
  set_harness_scenario_exit(fn(*shell));
  return harness_scenario_exit() == 0;
}

}  // namespace

int scenario_stormsurge(HarnessShell& browser) {
  return detail::run_stormsurge_scene3d(browser);
}

bool register_stormsurge_showcase(content::PluginHost* host) {
  register_stormsurge_interact_verbs();
  if (!host) {
    return false;
  }
  if (tool::CommandCatalog* catalog = host->commands()) {
    if (catalog->contains("stormsurge.scenario.showcase")) {
      return true;
    }
  }
  return host->contribute_command(
      kPluginId, "stormsurge.scenario.showcase", "Stormsurge Scene3D showcase",
      "tools", [host](const tool::CommandArgs&) {
        return run_bound(host, scenario_stormsurge);
      });
}

namespace {
struct StormsurgeInteractOnce {
  StormsurgeInteractOnce() { register_stormsurge_interact_verbs(); }
} k_stormsurge_interact_once;
}  // namespace

}  // namespace plugin
