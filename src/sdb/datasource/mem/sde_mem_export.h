// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_MEM_SDE_MEM_EXPORT_H_
#define SDB_DATASOURCE_MEM_SDE_MEM_EXPORT_H_

// GN defines SDE_MEM_EXPORTS when building the sde_mem DLL.
#if defined(SDE_MEM_EXPORTS)
#define SDE_MEM_EXPORT __declspec(dllexport)
#else
#define SDE_MEM_EXPORT __declspec(dllimport)
#endif

#if !defined(SDE_MEM_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "sde_memD.lib")
#else
#pragma comment(lib, "sde_mem.lib")
#endif
#endif

#endif  // SDB_DATASOURCE_MEM_SDE_MEM_EXPORT_H_
