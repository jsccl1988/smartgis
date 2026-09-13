// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_WS_SDE_WS_EXPORT_H_
#define SDB_DATASOURCE_WS_SDE_WS_EXPORT_H_

// GN defines SDE_WS_EXPORTS when building the sde_ws DLL (if enabled).
#if defined(SDE_WS_EXPORTS)
#define SDE_WS_EXPORT __declspec(dllexport)
#else
#define SDE_WS_EXPORT __declspec(dllimport)
#endif

#if !defined(SDE_WS_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "sde_wsD.lib")
#else
#pragma comment(lib, "sde_ws.lib")
#endif
#endif

#endif  // SDB_DATASOURCE_WS_SDE_WS_EXPORT_H_
