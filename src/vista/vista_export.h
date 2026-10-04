// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_VISTA_EXPORT_H_
#define VISTA_VISTA_EXPORT_H_

// GN defines VISTA_EXPORTS when building //src/vista:vista (dll_stem = vista).
#if defined(VISTA_EXPORTS)
#define VISTA_EXPORT __declspec(dllexport)
#else
#define VISTA_EXPORT __declspec(dllimport)
#endif

#if !defined(VISTA_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "vista_d.lib")
#else
#pragma comment(lib, "vista.lib")
#endif
#endif

#endif  // VISTA_VISTA_EXPORT_H_
