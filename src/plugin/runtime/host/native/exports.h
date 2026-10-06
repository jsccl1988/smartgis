// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_ABI_EXPORTS_H_
#define PLUGIN_RUNTIME_HOST_ABI_EXPORTS_H_

#include "plugin/runtime/host/plugin_host_export.h"

namespace plugin {

// Product native DLL ABI (mogu-shaped: load → init → run events → destroy).
// Export these C names from the plugin DLL. |host| is content::PluginHost*
// passed as void* so the export stays a C ABI (no C++ type in the .def).
//
//   int  init(void* host);                          // 0 = ok
//   int  run(const char* event_id, const char* payload);  // 0 unhandled, >0 ok
//   void destroy();
//
// Leftover AM names (StartPlugin / StopPlugin) remain fallbacks in NativeModule.
using PluginInitFn = int (*)(void* host);
using PluginRunFn = int (*)(const char* event_id, const char* payload);
using PluginDestroyFn = void (*)();

inline constexpr const char* kExportInit = "init";
inline constexpr const char* kExportRun = "run";
inline constexpr const char* kExportDestroy = "destroy";

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_ABI_EXPORTS_H_
