// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_NATIVE_SCAN_H_
#define PLUGIN_RUNTIME_HOST_NATIVE_SCAN_H_

#include <string>
#include <vector>

#include "plugin/runtime/host/catalog/manifest.h"
#include "plugin/runtime/host/plugin_host_export.h"

namespace plugin {

// One package found under --plugins-dir (<exe>/plugins).
struct DiscoveredPlugin {
  Manifest manifest;
  std::string directory;
  std::string dll_path;
};

// POD copy of plugin.json `startup` for EXE/DLL callers. Do not return
// DiscoveredPlugin / std::string across the plugin_host DLL boundary
// (MSVC debug STL iterator proxies are not ABI-safe).
struct PluginStartupPeek {
  char scenario[81];
  char present[33];
  char fields[513];
};

// One-level scan: each subdirectory may have plugin.json and/or a DLL.
// Same-module only (plugin_host DLL or tests linked with host_sources).
std::vector<DiscoveredPlugin> scan_plugins_dir(const std::string& root,
                                               std::string* err);

// Highest-priority activate pack: scenario / present / fields. Safe to call
// from SmartGIS.exe.
PLUGIN_HOST_EXPORT void peek_plugins_dir_startup(const char* plugins_dir,
                                                 PluginStartupPeek* out);

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_NATIVE_SCAN_H_
