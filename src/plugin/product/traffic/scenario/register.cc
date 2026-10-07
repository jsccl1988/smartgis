// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/traffic/scenario/register.h"

#include "content/public/plugin_host.h"
#include "plugin/product/traffic/commands.h"
#include "plugin/product/traffic/scenario/run.h"
#include "plugin/runtime/host/capability/scenario_shell.h"
#include "plugin/runtime/host/capability/scenario_command.h"
#include "plugin/runtime/host/capability/pack_ensure.h"
#include "tool/command/command.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.traffic";

bool ensure_traffic_pack(content::PluginHost* host) {
  return register_traffic(host) && register_traffic_scenario(host);
}

struct TrafficHarnessOnce {
  TrafficHarnessOnce() {
    register_command_pack("traffic", ensure_traffic_pack);
  }
} k_traffic_harness_once;

}  // namespace

int scenario_traffic(HarnessShell& browser) {
  return detail::run_traffic(browser);
}

bool register_traffic_scenario(content::PluginHost* host) {
  return contribute_scenario_command(
      host, kPluginId, "traffic.scenario.showcase", "Traffic Map2d showcase",
      scenario_traffic, detail::bind_plugin_scenario_shell);
}

}  // namespace plugin
