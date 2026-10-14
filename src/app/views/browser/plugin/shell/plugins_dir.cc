// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/plugin/shell/plugin_shell.h"

#include <cstdio>
#include <windows.h>

#include "app/views/util/exe_sidecar_path.h"
#include "plugin/runtime/host/native/scan.h"

namespace app {

std::string default_plugins_dir() {
  char path[MAX_PATH] = {};
  // <exe_dir>/plugins  (out/Debug/plugins or out/Release/plugins)
  if (!detail::exe_sidecar_path_a(path, MAX_PATH, "plugins")) {
    return {};
  }
  return std::string(path);
}

void peek_plugin_startup(const std::string& plugins_dir,
                         std::string* scenario_id,
                         std::string* plugin_present,
                         std::string* atmosphere_fields) {
  if (scenario_id) {
    scenario_id->clear();
  }
  if (plugin_present) {
    plugin_present->clear();
  }
  if (atmosphere_fields) {
    atmosphere_fields->clear();
  }
  std::string root = plugins_dir.empty() ? default_plugins_dir() : plugins_dir;
  if (root.empty()) {
    return;
  }
  // #region agent log
  {
    FILE* f = nullptr;
    if (fopen_s(&f, "C:/Dev/src/gis/smartgis/debug-778a24.log", "a") == 0 &&
        f) {
      std::fprintf(
          f,
          "{\"sessionId\":\"778a24\",\"runId\":\"pre\",\"hypothesisId\":\"C\","
          "\"location\":\"plugins_dir.cc:peek\",\"message\":\"peek_plugin_"
          "startup\",\"data\":{\"root\":\"%s\"},\"timestamp\":0}\n",
          root.c_str());
      std::fclose(f);
    }
  }
  // #endregion
  plugin::PluginStartupPeek peek{};
  plugin::peek_plugins_dir_startup(root.c_str(), &peek);
  if (scenario_id && peek.scenario[0]) {
    *scenario_id = peek.scenario;
  }
  if (plugin_present && peek.present[0]) {
    *plugin_present = peek.present;
  }
  if (atmosphere_fields && peek.fields[0]) {
    *atmosphere_fields = peek.fields;
  }
}

}  // namespace app
