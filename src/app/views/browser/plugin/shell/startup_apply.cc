// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/plugin/shell/startup_apply.h"

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/catalog/registry.h"

namespace app {
namespace detail {

bool apply_registry_startup(plugin::Registry* registry,
                            content::PluginHost* host,
                            std::string* startup_viewport,
                            std::string* startup_scenario) {
  if (!registry || !host || !startup_viewport || !startup_scenario) {
    return false;
  }
  startup_viewport->clear();
  startup_scenario->clear();
  // Viewport/scenario are filled inside plugin_host.dll (no Manifest/std::string
  // assign across the EXE↔DLL boundary).
  char viewport[128] = {};
  char scenario[128] = {};
  const bool ok =
      registry->apply_startup(host, viewport, sizeof(viewport), scenario,
                              sizeof(scenario));
  if (viewport[0]) {
    *startup_viewport = viewport;
  }
  if (scenario[0]) {
    *startup_scenario = scenario;
  }
  return ok;
}

}  // namespace detail
}  // namespace app
