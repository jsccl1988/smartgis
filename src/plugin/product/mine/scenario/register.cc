// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/mine/scenario/register.h"

#include "content/public/plugin_host.h"
#include "plugin/product/mine/commands.h"
#include "plugin/product/mine/scenario/interact.h"
#include "plugin/product/mine/scenario/run.h"
#include "plugin/runtime/host/capability/scenario_shell.h"
#include "plugin/runtime/host/capability/scenario_command.h"
#include "plugin/runtime/host/capability/pack_ensure.h"
#include "tool/command/command.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.mine";

bool ensure_mine_pack(content::PluginHost* host) {
  // register_mine may already have run via the commands PackOnce; do not
  // short-circuit scenario contribution on a duplicate-command false.
  (void)register_mine(host);
  return register_mine_scenario(host);
}

struct MineHarnessOnce {
  MineHarnessOnce() {
    register_mine_interact_ops();
    register_command_pack("mine", ensure_mine_pack);
  }
} k_mine_harness_once;

}  // namespace

int scenario_mine(HarnessShell& browser) {
  return detail::run_mine_scene3d(browser);
}

bool register_mine_scenario(content::PluginHost* host) {
  return contribute_scenario_command(host, kPluginId, "mine.scenario.showcase",
                                     "Mine Scene3D showcase", scenario_mine,
                                     detail::bind_plugin_scenario_shell);
}

}  // namespace plugin
