// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_PROCESSING_PROCESSING_PHASE_H_
#define PLUGIN_RUNTIME_HOST_PROCESSING_PROCESSING_PHASE_H_

#include <functional>
#include <string_view>

#include "plugin/runtime/host/plugin_host_export.h"

namespace content {
class PluginHost;
}

namespace plugin {

// Compute runs on the ProcessingPool worker with a null host (no GIS / Views /
// HWND). Present re-enters on the UI drain thread with the real host.
// attach_host_processing wires factory(nullptr) then factory(host); product
// packs should use this dispatcher instead of hand-rolled if (host) branches.
using ProcessingComputeFn = std::function<bool(std::string_view args_json)>;
using ProcessingPresentFn = std::function<bool(content::PluginHost* host)>;

PLUGIN_HOST_EXPORT bool run_processing_phases(
    content::PluginHost* host,
    std::string_view args_json,
    const ProcessingComputeFn& compute,
    const ProcessingPresentFn& present);

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_PROCESSING_PROCESSING_PHASE_H_
