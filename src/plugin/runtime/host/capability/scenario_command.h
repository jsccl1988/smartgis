// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_CAPABILITY_SCENARIO_COMMAND_H_
#define PLUGIN_RUNTIME_HOST_CAPABILITY_SCENARIO_COMMAND_H_

#include <string_view>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/scenario.h"
#include "plugin/runtime/host/capability/shell.h"
#include "tool/command/command.h"

namespace plugin {

using HarnessScenarioFn = int (*)(HarnessShell&);
// Optional TLS / mark binder (detail::bind_plugin_scenario_shell in
// capability/scenario_shell.h).
using HarnessPrepFn = void (*)(HarnessShell*);

inline bool run_harness_bound(content::PluginHost* host,
                              HarnessScenarioFn fn,
                              HarnessPrepFn prep = nullptr) {
  HarnessShell* shell = harness_shell(host);
  if (!shell || !fn) {
    set_harness_scenario_exit(1);
    return false;
  }
  if (prep) {
    prep(shell);
  }
  set_harness_scenario_exit(fn(*shell));
  return harness_scenario_exit() == 0;
}

// HWND / GIS harness scenario command (exit via harness_scenario_exit).
inline bool contribute_scenario_command(content::PluginHost* host,
                                        std::string_view plugin_id,
                                        std::string_view command_id,
                                        std::string_view title,
                                        HarnessScenarioFn fn,
                                        HarnessPrepFn prep = nullptr) {
  if (!host || plugin_id.empty() || !fn) {
    return false;
  }
  // Prefer contribute_command idempotency (handlers_) over catalog->contains;
  // a skewed CommandCatalog map AVs in tool_d find after cross-module emplace.
  return host->contribute_command(
      plugin_id, command_id, title, "tools",
      [host, fn, prep](const tool::CommandArgs&) {
        return run_harness_bound(host, fn, prep);
      });
}

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_CAPABILITY_SCENARIO_COMMAND_H_
