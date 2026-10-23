// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_PLUGIN_HOST_EXPORT_H_
#define PLUGIN_RUNTIME_HOST_PLUGIN_HOST_EXPORT_H_

// GN defines PLUGIN_HOST_EXPORTS when building the plugin host DLL
// (dll_stem = plugin_host).
// When is_component_build=false, COMPONENT_BUILD is unset → no-op.

#if defined(COMPONENT_BUILD)
#if defined(PLUGIN_HOST_EXPORTS)
#define PLUGIN_HOST_EXPORT __declspec(dllexport)
#else
#define PLUGIN_HOST_EXPORT __declspec(dllimport)
#endif
#else
#define PLUGIN_HOST_EXPORT
#endif

#if defined(COMPONENT_BUILD) && !defined(PLUGIN_HOST_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "plugin_host_d.lib")
#else
#pragma comment(lib, "plugin_host.lib")
#endif
#endif

#endif  // PLUGIN_RUNTIME_HOST_PLUGIN_HOST_EXPORT_H_
