// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_GIS_EXPORT_H_
#define SDB_GIS_EXPORT_H_

// GN defines GIS_EXPORTS when building the gis DLL.
#if defined(GIS_EXPORTS)
#define GIS_EXPORT __declspec(dllexport)
#else
#define GIS_EXPORT __declspec(dllimport)
#endif

#if !defined(GIS_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "gisD.lib")
#else
#pragma comment(lib, "gis.lib")
#endif
#endif

#endif  // SDB_GIS_EXPORT_H_
