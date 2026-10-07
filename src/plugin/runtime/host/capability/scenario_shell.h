// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_CAPABILITY_SCENARIO_SHELL_H_
#define PLUGIN_RUNTIME_HOST_CAPABILITY_SCENARIO_SHELL_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "plugin/runtime/host/plugin_host_export.h"

namespace content {
class PluginHost;
}

namespace plugin {

class HarnessShell;

namespace detail {

// Default HWND present size for Scene3D plugin / atmosphere showcases.
inline constexpr uint32_t kPluginPresentW = 640;
inline constexpr uint32_t kPluginPresentH = 480;

// Thread-local HarnessShell for plugin showcase marks / path helpers.
// Pass as HarnessPrepFn to contribute_scenario_command.
PLUGIN_HOST_EXPORT void bind_plugin_scenario_shell(HarnessShell* shell);
PLUGIN_HOST_EXPORT HarnessShell* plugin_scenario_shell();
PLUGIN_HOST_EXPORT void plugin_mark(const char* step);

// Separate TLS binder for atmosphere showcase marks (kMarkAtmosphere).
PLUGIN_HOST_EXPORT void bind_atmosphere_scenario_shell(HarnessShell* shell);
PLUGIN_HOST_EXPORT HarnessShell* atmosphere_scenario_shell();
PLUGIN_HOST_EXPORT void atmosphere_mark(const char* step);

// First existing relative path under the running exe directory (UTF-8 out).
PLUGIN_HOST_EXPORT bool resolve_rel_under_exe(const wchar_t* const* rels,
                                              size_t count, char* out_utf8,
                                              size_t out_cap);

// Escape backslash and quote for embedding a path in a JSON string value.
PLUGIN_HOST_EXPORT std::string json_escape_path(const char* path);

// PluginHost::run_processing only enqueues; GisScene / GisDocument present
// runs on ProcessingPool::flush_for_test. Showcase seeds must drain first.
PLUGIN_HOST_EXPORT bool run_processing_flushed(content::PluginHost* host,
                                               const char* id,
                                               const std::string& args);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_CAPABILITY_SCENARIO_SHELL_H_
