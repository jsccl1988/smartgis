// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/event_bus.h"
#include "content/public/plugin_host.h"
#include "plugin/product/map2d/scenario/hwnd_register.h"
#include "plugin/product/map2d/scenario/register.h"
#include "tool/command/command.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {
int g_fails = 0;
void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}
}  // namespace

int main() {
  tool::CommandCatalog catalog;
  content::EventBus events;
  content::PluginHost* host =
      content::create_plugin_host(&catalog, &events, nullptr);
  expect(host != nullptr, "create_plugin_host");
  if (!host) {
    return 1;
  }
  expect(plugin::register_map2d_scenarios(host), "register_map2d_scenarios");
  expect(plugin::register_map2d_scenario(host), "register_map2d_scenario");

  std::vector<std::string> ids;
  host->for_each_command(
      [&](std::string_view, std::string_view command_id, std::string_view) {
        ids.emplace_back(command_id);
      });
  auto has = [&](const char* id) {
    for (const std::string& s : ids) {
      if (s == id) {
        return true;
      }
    }
    return false;
  };
  expect(has("map2d.scenario.edit_m0"), "map2d.scenario.edit_m0");
  expect(has("map2d.scenario.layers_m1"), "map2d.scenario.layers_m1");
  expect(has("map2d.scenario.navigate"), "map2d.scenario.navigate");
  expect(has("map2d.scenario.present"), "map2d.scenario.present");
  expect(has("map2d.scenario.milestones"), "map2d.scenario.milestones");
  expect(has("map2d.scenario.china"), "map2d.scenario.china");
  expect(has("map2d.scenario.align"), "map2d.scenario.align");
  expect(has("map2d.scenario.orthogrid"), "map2d.scenario.orthogrid");
  expect(has("map2d.scenario.print"), "map2d.scenario.print");

  expect(!host->execute("map2d.scenario.edit_m0", tool::CommandArgs{}),
         "run without shell fails");
  expect(plugin::scenario_last_exit_code() != 0, "last_rc set");
  expect(!host->execute("map2d.scenario.china", tool::CommandArgs{}),
         "china without shell fails");

  delete host;
  if (g_fails) {
    std::fprintf(stderr, "%d FAIL(s)\n", g_fails);
    return 1;
  }
  return 0;
}
