// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/hwnd_register.h"

#include "content/public/plugin_host.h"
#include "plugin/product/map2d/scenario/interact.h"
#include "plugin/runtime/host/capability/scenario.h"
#include "plugin/runtime/host/capability/shell.h"
#include "tool/command/command.h"

#include <string_view>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.map2d";

bool run_bound(content::PluginHost* host, int (*fn)(HarnessShell&)) {
  HarnessShell* shell = harness_shell(host);
  if (!shell || !fn) {
    set_harness_scenario_exit(1);
    return false;
  }
  set_harness_scenario_exit(fn(*shell));
  return harness_scenario_exit() == 0;
}

bool contribute_one(content::PluginHost* host, std::string_view command_id,
                    std::string_view title, int (*fn)(HarnessShell&)) {
  return host->contribute_command(
      kPluginId, command_id, title, "tools",
      [host, fn](const tool::CommandArgs&) { return run_bound(host, fn); });
}

}  // namespace

bool register_map2d_showcase(content::PluginHost* host) {
  register_map2d_interact_verbs();
  if (!host) {
    return false;
  }
  if (tool::CommandCatalog* catalog = host->commands()) {
    if (catalog->contains("map2d.scenario.china")) {
      return true;
    }
  }
  return contribute_one(host, "map2d.scenario.china", "Map2d china showcase",
                        scenario_china) &&
         contribute_one(host, "map2d.scenario.align", "Map2d align showcase",
                        scenario_align) &&
         contribute_one(host, "map2d.scenario.orthogrid",
                        "Map2d orthogrid showcase", scenario_orthogrid) &&
         contribute_one(host, "map2d.scenario.print", "Map2d print showcase",
                        scenario_print);
}

namespace {
struct Map2dInteractOnce {
  Map2dInteractOnce() { register_map2d_interact_verbs(); }
} k_map2d_interact_once;
}  // namespace

}  // namespace plugin
