// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENIC_EXPORT_H_
#define SCENIC_SCENIC_EXPORT_H_

// GN defines SCENIC_EXPORTS when building //src/scenic:scenic (dll_stem = scenic).
#if defined(SCENIC_EXPORTS)
#define SCENIC_EXPORT __declspec(dllexport)
#else
#define SCENIC_EXPORT __declspec(dllimport)
#endif

#if !defined(SCENIC_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_d.lib")
#else
#pragma comment(lib, "scenic.lib")
#endif
#endif

#endif  // SCENIC_SCENIC_EXPORT_H_
