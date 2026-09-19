// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_FEATURE_LEFTOVER_ATTR_EXPORT_H_
#define SDB_FEATURE_LEFTOVER_ATTR_EXPORT_H_

// Export for //src/gis/feature:leftover_attr (not //src/gis/map:gis).
#if defined(LEFTOVER_ATTR_EXPORTS)
#define LEFTOVER_ATTR_EXPORT __declspec(dllexport)
#else
#define LEFTOVER_ATTR_EXPORT __declspec(dllimport)
#endif

#if !defined(LEFTOVER_ATTR_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "leftover_attr_d.lib")
#else
#pragma comment(lib, "leftover_attr.lib")
#endif
#endif

#endif  // SDB_FEATURE_LEFTOVER_ATTR_EXPORT_H_
