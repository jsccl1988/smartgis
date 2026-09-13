// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_SMF_SDE_SMF_EXPORT_H_
#define SDB_DATASOURCE_SMF_SDE_SMF_EXPORT_H_

// GN defines SDE_SMF_EXPORTS when building the sde_smf DLL (if enabled).
#if defined(SDE_SMF_EXPORTS)
#define SDE_SMF_EXPORT __declspec(dllexport)
#else
#define SDE_SMF_EXPORT __declspec(dllimport)
#endif

#if !defined(SDE_SMF_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "sde_smfD.lib")
#else
#pragma comment(lib, "sde_smf.lib")
#endif
#endif

#endif  // SDB_DATASOURCE_SMF_SDE_SMF_EXPORT_H_
