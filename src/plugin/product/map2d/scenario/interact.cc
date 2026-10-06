// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/interact.h"

#include <string>
#include <string_view>

#include "content/browser/capability/host.h"
#include "plugin/runtime/host/capability/scenario.h"

namespace plugin {
namespace {

bool exec_map2d_run(content::CapabilityHost& host, std::string_view mode) {
  return host.map2d_run && host.map2d_run(std::string(mode));
}

bool exec_map2d_orthogrid_run(content::CapabilityHost& host, std::string_view) {
  return host.run_plugin_command &&
         host.run_plugin_command("map2d.scenario.orthogrid");
}

bool exec_map2d_print_run(content::CapabilityHost& host, std::string_view) {
  return host.run_plugin_command &&
         host.run_plugin_command("map2d.scenario.print");
}

}  // namespace

void register_map2d_interact_verbs() {
  register_showcase_verb("map2d_run", exec_map2d_run, "china");
  register_showcase_verb("map2d_orthogrid_run", exec_map2d_orthogrid_run, "");
  register_showcase_verb("map2d_print_run", exec_map2d_print_run, "");
}

}  // namespace plugin
