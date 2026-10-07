// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/hwnd_register.h"

#include "content/public/plugin_host.h"
#include "plugin/product/map2d/commands.h"
#include "plugin/product/map2d/scenario/interact.h"
#include "plugin/product/map2d/scenario/register.h"
#include "plugin/runtime/host/capability/scenario_command.h"
#include "plugin/runtime/host/capability/pack_ensure.h"
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

bool ensure_map2d_pack(content::PluginHost* host) {
  return register_map2d(host) && register_map2d_scenario(host) &&
         register_map2d_scenarios(host);
}

struct Map2dHarnessOnce {
  Map2dHarnessOnce() {
    register_map2d_interact_ops();
    register_command_pack("map2d", ensure_map2d_pack);
    register_command_pack("print.", ensure_map2d_pack);
  }
} k_map2d_harness_once;

}  // namespace

bool register_map2d_scenario(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  if (tool::CommandCatalog* catalog = host->commands()) {
    if (catalog->contains("map2d.scenario.china")) {
      return true;
    }
  }
  const ScenarioCmd cmds[] = {
      {"map2d.scenario.china", "Map2d china showcase", scenario_china},
      {"map2d.scenario.align", "Map2d align showcase", scenario_align},
      {"map2d.scenario.orthogrid", "Map2d orthogrid showcase",
       scenario_orthogrid},
      {"map2d.scenario.print", "Map2d print showcase", scenario_print},
  };
  for (const ScenarioCmd& c : cmds) {
    if (!contribute_scenario_command(host, kPluginId, c.id, c.title, c.fn)) {
      return false;
    }
  }
  return true;
}

}  // namespace plugin
