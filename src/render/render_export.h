// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_RENDER_EXPORT_H_
#define RENDER_RENDER_EXPORT_H_

// GN defines RENDER_EXPORTS when building the render DLL (dll_stem = render).
#if defined(RENDER_EXPORTS)
#define RENDER_EXPORT __declspec(dllexport)
#else
#define RENDER_EXPORT __declspec(dllimport)
#endif

#if !defined(RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "render_d.lib")
#else
#pragma comment(lib, "render.lib")
#endif
#endif

#endif  // RENDER_RENDER_EXPORT_H_
