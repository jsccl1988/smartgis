// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_UI_VIEWS_EXPORT_H_
#define UI_UI_VIEWS_EXPORT_H_

// Single export for //src/ui/views:ui_views (dll_stem = ui_views).
// Covers both ui::views and ui::gfx (one PE). GN sets UI_VIEWS_EXPORTS on
// the source_sets compiled into that DLL.

#if defined(UI_VIEWS_EXPORTS)
#define UI_VIEWS_EXPORT __declspec(dllexport)
#else
#define UI_VIEWS_EXPORT __declspec(dllimport)
#endif

#if !defined(UI_VIEWS_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_views_d.lib")
#else
#pragma comment(lib, "ui_views.lib")
#endif
#endif

#endif  // UI_UI_VIEWS_EXPORT_H_
