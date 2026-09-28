// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_UI_EXPORT_H_
#define UI_UI_EXPORT_H_

// Single export for //src/ui (dll_stem = ui_views).
// Covers ui::views, ui::gfx, and product GIS chrome under ui/gis (one PE).
// GN sets UI_EXPORTS on the source_sets compiled into that DLL.

#if defined(UI_EXPORTS)
#define UI_EXPORT __declspec(dllexport)
#else
#define UI_EXPORT __declspec(dllimport)
#endif

#if !defined(UI_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_views_d.lib")
#else
#pragma comment(lib, "ui_views.lib")
#endif
#endif

#endif  // UI_UI_EXPORT_H_
