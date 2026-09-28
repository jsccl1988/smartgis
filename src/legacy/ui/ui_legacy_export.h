// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_UI_LEGACY_EXPORT_H_
#define LEGACY_UI_UI_LEGACY_EXPORT_H_

// Single export for //src/legacy/ui:ui_legacy (dll_stem = ui_legacy).
// tool/group objects are compiled into this DLL as well.

#if defined(UI_LEGACY_EXPORTS)
#define UI_LEGACY_EXPORT __declspec(dllexport)
#else
#define UI_LEGACY_EXPORT __declspec(dllimport)
#endif

#endif  // LEGACY_UI_UI_LEGACY_EXPORT_H_
