// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/interact.h"

#include <string>
#include <string_view>

#include "content/browser/capability/host.h"
#include "plugin/runtime/host/capability/scenario.h"

namespace plugin {
namespace {

bool exec_atmosphere_run(content::CapabilityHost& host, std::string_view mode) {
  return host.plugin.atmosphere_run &&
         host.plugin.atmosphere_run(std::string(mode));
}

bool exec_world3d_run(content::CapabilityHost& host, std::string_view) {
  return host.plugin.run_plugin_command &&
         host.plugin.run_plugin_command("world3d.scenario.showcase");
}

bool exec_orthogrid3d_run(content::CapabilityHost& host, std::string_view) {
  return host.plugin.run_plugin_command &&
         host.plugin.run_plugin_command("orthogrid3d.scenario.showcase");
}

bool exec_orthogrid_run(content::CapabilityHost& host, std::string_view) {
  return host.plugin.run_plugin_command &&
         host.plugin.run_plugin_command("orthogrid.scenario.showcase");
}

}  // namespace

void register_world3d_interact_verbs() {
  register_scenario_verb("atmosphere_run", exec_atmosphere_run, "full");
  register_scenario_verb("world3d_run", exec_world3d_run, "");
  register_scenario_verb("orthogrid3d_run", exec_orthogrid3d_run, "");
  register_scenario_verb("orthogrid_run", exec_orthogrid_run, "");
}

}  // namespace plugin
