// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/traffic/scenario/register.h"

#include "content/public/plugin_host.h"
#include "plugin/product/traffic/scenario/run.h"
#include "plugin/product/world3d/scenario/common/plugin_io.h"
#include "plugin/runtime/host/capability/scenario.h"
#include "plugin/runtime/host/capability/shell.h"
#include "tool/command/command.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.traffic";

bool run_bound(content::PluginHost* host, int (*fn)(HarnessShell&)) {
  HarnessShell* shell = harness_shell(host);
  if (!shell || !fn) {
    set_harness_scenario_exit(1);
    return false;
  }
  detail::bind_plugin_showcase_shell(shell);
  set_harness_scenario_exit(fn(*shell));
  return harness_scenario_exit() == 0;
}

}  // namespace

int scenario_traffic(HarnessShell& browser) {
  return detail::run_traffic(browser);
}

bool register_traffic_showcase(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  if (tool::CommandCatalog* catalog = host->commands()) {
    if (catalog->contains("traffic.scenario.showcase")) {
      return true;
    }
  }
  return host->contribute_command(
      kPluginId, "traffic.scenario.showcase", "Traffic Map2d showcase", "tools",
      [host](const tool::CommandArgs&) {
        return run_bound(host, scenario_traffic);
      });
}

}  // namespace plugin
