// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_LEGACY_RENDER_EXPORT_H_
#define LEGACY_RENDER_LEGACY_RENDER_EXPORT_H_

// Single export for //src/legacy/render:legacy_render (dll_stem =
// legacy_render). Do not key this off RENDER_EXPORTS — that belongs to
// //src/render.

#if defined(DEM_HEIGHT_FIELD_STATIC)
#define LEGACY_RENDER_EXPORT
#elif defined(LEGACY_RENDER_EXPORTS)
#define LEGACY_RENDER_EXPORT __declspec(dllexport)
#else
#define LEGACY_RENDER_EXPORT __declspec(dllimport)
#endif

#endif  // LEGACY_RENDER_LEGACY_RENDER_EXPORT_H_
