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
  return host.atmosphere_run && host.atmosphere_run(std::string(mode));
}

bool exec_world3d_run(content::CapabilityHost& host, std::string_view) {
  return host.run_plugin_command &&
         host.run_plugin_command("world3d.scenario.showcase");
}

bool exec_orthogrid3d_run(content::CapabilityHost& host, std::string_view) {
  return host.run_plugin_command &&
         host.run_plugin_command("orthogrid3d.scenario.showcase");
}

bool exec_orthogrid_run(content::CapabilityHost& host, std::string_view) {
  return host.run_plugin_command &&
         host.run_plugin_command("orthogrid.scenario.showcase");
}

}  // namespace

void register_world3d_interact_verbs() {
  register_showcase_verb("atmosphere_run", exec_atmosphere_run, "full");
  register_showcase_verb("world3d_run", exec_world3d_run, "");
  register_showcase_verb("orthogrid3d_run", exec_orthogrid3d_run, "");
  register_showcase_verb("orthogrid_run", exec_orthogrid_run, "");
}

}  // namespace plugin
