// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_CAPABILITY_H_
#define PLUGIN_RUNTIME_HOST_CAPABILITY_H_

#include <string_view>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/report/bridge.h"
#include "plugin/runtime/host/capability/scene3d/sink.h"

namespace plugin {

class ProcessingPool;

// Reverse-DNS ids owned by plugin_host.dll / chrome. content::PluginHost stores
// only the string and an opaque pointer. Typed facades live in this directory
// (scene3d/, report/); ProcessingPool stays in host/processing/.
inline constexpr std::string_view kCapabilityScene3d = "plugin.scene3d";
inline constexpr std::string_view kCapabilityReport = "plugin.report";
inline constexpr std::string_view kCapabilityProcessingPool =
    "plugin.processing_pool";

inline Scene3dSink* scene3d_sink(content::PluginHost* host) {
  return host ? static_cast<Scene3dSink*>(
                    host->query_capability(kCapabilityScene3d))
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

// Shell-owned Scene3D + report facades. Attach after create_plugin_host.
struct HostCapabilities {
  Scene3dSink scene3d;
  ReportBridge report;

  void attach(content::PluginHost* host) {
    if (!host) {
      return;
    }
    host->set_capability(kCapabilityScene3d, &scene3d);
    host->set_capability(kCapabilityReport, &report);
  }

  void detach(content::PluginHost* host) {
    if (!host) {
      return;
    }
    host->set_capability(kCapabilityScene3d, nullptr);
    host->set_capability(kCapabilityReport, nullptr);
  }
};

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_CAPABILITY_H_
