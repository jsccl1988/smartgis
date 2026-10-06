// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/event_bus.h"
#include "content/public/plugin_host.h"
#include "plugin/product/self_test/commands.h"
#include "tool/command/command.h"

#include <cstdio>
#include <cstring>
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
  expect(plugin::register_self_test(host), "register_self_test");

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
  expect(has("self_test.run"), "self_test.run");
  expect(has("self_test.console"), "self_test.console");
  expect(has("self_test.shell_ready"), "self_test.shell_ready");
  expect(has("self_test.edit_m0"), "self_test.edit_m0");
  expect(has("self_test.layers_m1"), "self_test.layers_m1");
  expect(has("self_test.navigate"), "self_test.navigate");
  expect(has("self_test.present"), "self_test.present");
  expect(has("self_test.layout_bounds"), "self_test.layout_bounds");
  expect(has("self_test.milestones"), "self_test.milestones");

  expect(!host->execute("self_test.run", tool::CommandArgs{}),
         "run without shell fails");
  expect(plugin::self_test_last_exit_code() != 0, "last_rc set");

  delete host;
  if (g_fails) {
    std::fprintf(stderr, "%d FAIL(s)\n", g_fails);
    return 1;
  }
  return 0;
}
