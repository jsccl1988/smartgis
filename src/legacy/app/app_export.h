// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_APP_APP_EXPORT_H_
#define LEGACY_APP_APP_EXPORT_H_

// Single export for //src/legacy/app:app_core (dll_stem = app_core).

#if defined(APP_CORE_EXPORTS)
#define APP_CORE_EXPORT __declspec(dllexport)
#else
#define APP_CORE_EXPORT __declspec(dllimport)
#endif

#endif  // LEGACY_APP_APP_EXPORT_H_
