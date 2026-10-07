// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/register.h"

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/scenario_command.h"
#include "plugin/runtime/host/capability/scenario.h"
#include "tool/command/command.h"

#include <string_view>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.map2d";

struct ScenarioCmd {
  const char* id;
  const char* title;
  HarnessScenarioFn fn;
};

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
  const ScenarioCmd cmds[] = {
      {"map2d.scenario.edit_m0", "Map2d edit_m0 scenario", scenario_edit_m0},
      {"map2d.scenario.layers_m1", "Map2d layers_m1 scenario",
       scenario_layers_m1},
      {"map2d.scenario.navigate", "Map2d navigate scenario", scenario_navigate},
      {"map2d.scenario.present", "Map2d present scenario", scenario_present},
      {"map2d.scenario.milestones", "Map2d milestones scenario",
       scenario_milestones},
  };
  for (const ScenarioCmd& c : cmds) {
    if (!contribute_scenario_command(host, kPluginId, c.id, c.title, c.fn)) {
      return false;
    }
  }
  return host->contribute_processing(
      kPluginId, {"map2d.scenario.edit_m0", "Run map2d edit_m0 harness payload"},
      [](content::PluginHost* h, std::string_view) {
        return run_harness_bound(h, scenario_edit_m0);
      });
}

}  // namespace plugin
