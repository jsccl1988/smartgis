// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_MGR_SDE_MGR_EXPORT_H_
#define SDB_DATASOURCE_MGR_SDE_MGR_EXPORT_H_

// GN defines SDE_MGR_EXPORTS when building the sde_mgr DLL.
#if defined(SDE_MGR_EXPORTS)
#define SDE_MGR_EXPORT __declspec(dllexport)
#else
#define SDE_MGR_EXPORT __declspec(dllimport)
#endif

#if !defined(SDE_MGR_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "sde_mgrD.lib")
#else
#pragma comment(lib, "sde_mgr.lib")
#endif
#endif

#endif  // SDB_DATASOURCE_MGR_SDE_MGR_EXPORT_H_
