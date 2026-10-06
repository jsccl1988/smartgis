// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/register.h"

#include "content/public/plugin_host.h"
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

bool contribute_one(content::PluginHost* host,
                    std::string_view command_id,
                    std::string_view title,
                    int (*fn)(HarnessShell&)) {
  return host->contribute_command(
      kPluginId, command_id, title, "tools",
      [host, fn](const tool::CommandArgs&) { return run_bound(host, fn); });
}

}  // namespace

int scenario_last_exit_code() {
  return harness_scenario_exit();
}

bool register_map2d_scenarios(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  if (tool::CommandCatalog* catalog = host->commands()) {
    if (catalog->contains("map2d.scenario.edit_m0")) {
      return true;
    }
  }
  if (!contribute_one(host, "map2d.scenario.edit_m0", "Map2d edit_m0 scenario",
                      scenario_edit_m0) ||
      !contribute_one(host, "map2d.scenario.layers_m1",
                      "Map2d layers_m1 scenario", scenario_layers_m1) ||
      !contribute_one(host, "map2d.scenario.navigate",
                      "Map2d navigate scenario", scenario_navigate) ||
      !contribute_one(host, "map2d.scenario.present", "Map2d present scenario",
                      scenario_present) ||
      !contribute_one(host, "map2d.scenario.milestones",
                      "Map2d milestones scenario", scenario_milestones)) {
    return false;
  }
  return host->contribute_processing(
      kPluginId, {"map2d.scenario.edit_m0", "Run map2d edit_m0 harness payload"},
      [](content::PluginHost* h, std::string_view) {
        return run_bound(h, scenario_edit_m0);
      });
}

}  // namespace plugin
