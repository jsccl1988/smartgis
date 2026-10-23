// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_UI_EXPORT_H_
#define UI_UI_EXPORT_H_

// Single export for //src/ui (dll_stem = ui_views).
// Covers ui::views, ui::gfx, and product GIS horizon under ui/gis (one PE).
// GN sets UI_EXPORTS on the source_sets compiled into that DLL.
// When is_component_build=false, COMPONENT_BUILD is unset → no-op.

#if defined(COMPONENT_BUILD)
#if defined(UI_EXPORTS)
#define UI_EXPORT __declspec(dllexport)
#else
#define UI_EXPORT __declspec(dllimport)
#endif
#else
#define UI_EXPORT
#endif

#if defined(COMPONENT_BUILD) && !defined(UI_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_views_d.lib")
#else
#pragma comment(lib, "ui_views.lib")
#endif
#endif

#endif  // UI_UI_EXPORT_H_
