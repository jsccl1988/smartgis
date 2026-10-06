// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/mine/scenario/interact.h"

#include <string_view>

#include "content/browser/capability/host.h"
#include "plugin/runtime/host/capability/scenario.h"

namespace plugin {
namespace {

bool exec_mine_run(content::CapabilityHost& host, std::string_view) {
  return host.plugin.run_plugin_command &&
         host.plugin.run_plugin_command("mine.scenario.showcase");
}

}  // namespace

void register_mine_interact_verbs() {
  register_scenario_verb("mine_run", exec_mine_run, "");
}

}  // namespace plugin
