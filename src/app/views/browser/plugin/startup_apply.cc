// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/plugin/startup_apply.h"

#include <vector>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/catalog/registry.h"
#include "tool/command/command.h"

namespace app {
namespace detail {

bool apply_registry_startup(plugin::Registry* registry,
                            content::PluginHost* host,
                            std::string* startup_viewport,
                            std::string* startup_scenario) {
  if (!registry || !host || !startup_viewport || !startup_scenario) {
    return false;
  }

  int best_viewport = 0;
  int best_scenario = 0;
  bool have_viewport = false;
  bool have_scenario = false;
  startup_viewport->clear();
  startup_scenario->clear();

  const std::vector<plugin::PluginRecord> rows = registry->list();
  for (const plugin::PluginRecord& rec : rows) {
    if (rec.trust == plugin::TrustClass::kDenied) {
      continue;
    }
    const plugin::ManifestStartup& st = rec.manifest.startup;
    if (!st.activate) {
      continue;
    }
    if (!st.viewport.empty() &&
        (!have_viewport || st.priority > best_viewport)) {
      have_viewport = true;
      best_viewport = st.priority;
      *startup_viewport = st.viewport;
    }
    if (!st.scenario.empty() &&
        (!have_scenario || st.priority > best_scenario)) {
      have_scenario = true;
      best_scenario = st.priority;
      *startup_scenario = st.scenario;
    }
    if (!st.commands.empty() || !st.seed.empty()) {
      (void)registry->set_enabled(rec.manifest.id, true, host);
    }
    for (const std::string& cmd : st.commands) {
      if (cmd.empty()) {
        continue;
      }
      (void)host->execute(cmd, tool::CommandArgs{});
    }
    if (!st.seed.empty()) {
      const int face = st.viewport == "scene3d" ? 1 : 0;
      (void)host->present_dataset(rec.manifest.id, st.seed, face);
    }
  }
  return true;
}

}  // namespace detail
}  // namespace app
