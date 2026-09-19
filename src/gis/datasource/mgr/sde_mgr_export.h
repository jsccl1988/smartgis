// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_MGR_SDE_MGR_EXPORT_H_
#define SDB_DATASOURCE_MGR_SDE_MGR_EXPORT_H_

// GN defines SDE_MGR_EXPORTS when building the sdb DLL (dll_stem = sdb).
#if defined(SDE_MGR_EXPORTS) || defined(SDB_EXPORTS)
#define SDE_MGR_EXPORT __declspec(dllexport)
#else
#define SDE_MGR_EXPORT __declspec(dllimport)
#endif

#if !defined(SDE_MGR_EXPORTS) && !defined(SDB_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "gis_d.lib")
#else
#pragma comment(lib, "gis.lib")
#endif
#endif

#endif  // SDB_DATASOURCE_MGR_SDE_MGR_EXPORT_H_
