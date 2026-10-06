// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_MANAGER_H_
#define PLUGIN_RUNTIME_HOST_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "plugin/runtime/host/native/module.h"
#include "plugin/runtime/host/native/scan.h"
#include "plugin/runtime/host/plugin_host_export.h"
#include "plugin/runtime/host/catalog/registry.h"

namespace content {
class PluginHost;
}

namespace plugin {

// Chrome-owned (not a process singleton). Scans --plugins-dir, loads native
// DLLs, calls init / run / destroy. In-process builtins stay on Registry hooks.
class PLUGIN_HOST_EXPORT PluginManager {
 public:
  explicit PluginManager(Registry* registry);
  ~PluginManager();

  PluginManager(const PluginManager&) = delete;
  PluginManager& operator=(const PluginManager&) = delete;

  int scan_directory(const std::string& plugins_dir);
  bool init_all(content::PluginHost* host);
  int dispatch_event(std::string_view event_id, std::string_view payload);
  void destroy_all(content::PluginHost* host);
  const std::string& last_error() const { return last_error_; }
  const std::vector<DiscoveredPlugin>& discovered() const { return discovered_; }

 private:
  bool start_native(const PluginRecord& rec, content::PluginHost* host);
  void stop_native(const PluginRecord& rec);
  int run_native(const PluginRecord& rec, std::string_view event_id,
                 std::string_view payload);

  Registry* registry_ = nullptr;
  std::string last_error_;
  std::vector<DiscoveredPlugin> discovered_;
  std::map<std::string, std::unique_ptr<NativeModule>> modules_;
};

PLUGIN_HOST_EXPORT PluginManager* plugin_manager_new(Registry* registry);
PLUGIN_HOST_EXPORT void plugin_manager_delete(PluginManager* manager);

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_MANAGER_H_
