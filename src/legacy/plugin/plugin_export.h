// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_PLUGIN_PLUGIN_EXPORT_H_
#define LEGACY_PLUGIN_PLUGIN_EXPORT_H_

// Single export for the AuxModule host DLL (//src/legacy/plugin:plugin).
// Domain plugins (plugin_dem, plugin_print, …) stay separate LoadLibrary DLLs.

#if defined(PLUGIN_EXPORTS)
#define PLUGIN_EXPORT __declspec(dllexport)
#else
#define PLUGIN_EXPORT __declspec(dllimport)
#endif

#if !defined(PLUGIN_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "plugin_d.lib")
#else
#pragma comment(lib, "plugin.lib")
#endif
#endif

#endif  // LEGACY_PLUGIN_PLUGIN_EXPORT_H_
