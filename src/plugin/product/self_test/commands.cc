// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/self_test/commands.h"

#include "content/public/plugin_host.h"
#include "plugin/product/self_test/shell.h"
#include "tool/command/command.h"

#include <string_view>

namespace plugin {
namespace {

int g_last_exit = 1;

SelfTestShell* bound_shell(content::PluginHost* host) {
  return self_test_shell(host);
}

bool run_bound(content::PluginHost* host, int (*fn)(SelfTestShell&)) {
  SelfTestShell* shell = bound_shell(host);
  if (!shell || !fn) {
    g_last_exit = 1;
    return false;
  }
  g_last_exit = fn(*shell);
  return g_last_exit == 0;
}

bool contribute_one(content::PluginHost* host,
                    std::string_view command_id,
                    std::string_view title,
                    int (*fn)(SelfTestShell&)) {
  return host->contribute_command(
      kSelfTestPluginId, command_id, title, "tools",
      [host, fn](const tool::CommandArgs&) { return run_bound(host, fn); });
}

}  // namespace

SelfTestShell* self_test_shell(content::PluginHost* host) {
  return host ? static_cast<SelfTestShell*>(
                    host->query_capability(kCapabilitySelfTest))
              : nullptr;
}

int self_test_last_exit_code() {
  return g_last_exit;
}

bool register_self_test(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  if (tool::CommandCatalog* catalog = host->commands()) {
    if (catalog->contains("self_test.run")) {
      return true;
    }
  }
  if (!contribute_one(host, "self_test.run", "Views self-test",
                      run_self_test_all) ||
      !contribute_one(host, "self_test.console", "Console self-test",
                      run_self_test_console) ||
      !contribute_one(host, "self_test.shell_ready", "Self-test shell_ready",
                      self_test_shell_ready) ||
      !contribute_one(host, "self_test.edit_m0", "Self-test edit_m0",
                      self_test_edit_m0) ||
      !contribute_one(host, "self_test.layers_m1", "Self-test layers_m1",
                      self_test_layers_m1) ||
      !contribute_one(host, "self_test.navigate", "Self-test navigate",
                      self_test_navigate) ||
      !contribute_one(host, "self_test.present", "Self-test present",
                      self_test_present) ||
      !contribute_one(host, "self_test.layout_bounds",
                      "Self-test layout_bounds", self_test_layout_bounds) ||
      !contribute_one(host, "self_test.milestones", "Self-test milestones",
                      self_test_milestones)) {
    return false;
  }
  return host->contribute_processing(
      kSelfTestPluginId, {"self_test.run", "Run Views harness self-test"},
      [](content::PluginHost* h, std::string_view) {
        return run_bound(h, run_self_test_all);
      });
}

}  // namespace plugin
