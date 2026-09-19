// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_GIS_EXPORT_H_
#define GIS_GIS_EXPORT_H_

// GN defines GIS_EXPORTS when building the gis DLL (dll_stem = gis).
// SDB_EXPORTS remains accepted as a transitional synonym.
#if defined(GIS_EXPORTS) || defined(SDB_EXPORTS)
#define GIS_EXPORT __declspec(dllexport)
#else
#define GIS_EXPORT __declspec(dllimport)
#endif

#if !defined(GIS_EXPORTS) && !defined(SDB_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "gis_d.lib")
#else
#pragma comment(lib, "gis.lib")
#endif
#endif

#endif  // GIS_GIS_EXPORT_H_
