// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_CAPABILITY_H_
#define PLUGIN_RUNTIME_HOST_CAPABILITY_H_

#include <string_view>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/map2d_sink.h"
#include "plugin/runtime/host/capability/report_bridge.h"
#include "plugin/runtime/host/capability/scene3d_sink.h"
#include "plugin/runtime/host/capability/shell_ui.h"

namespace plugin {

class ProcessingPool;

// Reverse-DNS ids owned by plugin_host.dll / chrome. content::PluginHost stores
// only the string and an opaque pointer. Typed facades are the sibling
// headers in this directory (including HarnessShell). ProcessingPool
// stays in host/processing/.
inline constexpr std::string_view kCapabilityScene3d = "plugin.scene3d";
inline constexpr std::string_view kCapabilityMap2d = "plugin.map2d";
inline constexpr std::string_view kCapabilityReport = "plugin.report";
inline constexpr std::string_view kCapabilityShellUi = "plugin.ui.shell";
inline constexpr std::string_view kCapabilityProcessingPool =
    "plugin.processing_pool";

inline Scene3dSink* scene3d_sink(content::PluginHost* host) {
  return host ? static_cast<Scene3dSink*>(
                    host->query_capability(kCapabilityScene3d))
              : nullptr;
}

inline Map2dSink* map2d_sink(content::PluginHost* host) {
  return host ? static_cast<Map2dSink*>(
                    host->query_capability(kCapabilityMap2d))
              : nullptr;
}

inline ReportBridge* report_bridge(content::PluginHost* host) {
  return host ? static_cast<ReportBridge*>(
                    host->query_capability(kCapabilityReport))
              : nullptr;
}

inline ProcessingPool* processing_pool(content::PluginHost* host) {
  return host ? static_cast<ProcessingPool*>(
                    host->query_capability(kCapabilityProcessingPool))
              : nullptr;
}

inline ShellUiSink* shell_ui(content::PluginHost* host) {
  return host ? static_cast<ShellUiSink*>(
                    host->query_capability(kCapabilityShellUi))
              : nullptr;
}

// Shell-owned Scene3D + Map2d + report + inspector/debug facades. Attach after
// create_plugin_host.
struct HostCapabilities {
  Scene3dSink scene3d;
  Map2dSink map2d;
  ReportBridge report;
  ShellUiSink shell_ui;

  void attach(content::PluginHost* host) {
    if (!host) {
      return;
    }
    host->set_capability(kCapabilityScene3d, &scene3d);
    host->set_capability(kCapabilityMap2d, &map2d);
    host->set_capability(kCapabilityReport, &report);
    host->set_capability(kCapabilityShellUi, &shell_ui);
  }

  void detach(content::PluginHost* host) {
    if (!host) {
      return;
    }
    host->set_capability(kCapabilityScene3d, nullptr);
    host->set_capability(kCapabilityMap2d, nullptr);
    host->set_capability(kCapabilityReport, nullptr);
    host->set_capability(kCapabilityShellUi, nullptr);
  }
};

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_CAPABILITY_H_
