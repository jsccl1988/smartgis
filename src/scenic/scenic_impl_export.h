// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_LEGACY_RENDER_EXPORT_H_
#define LEGACY_RENDER_LEGACY_RENDER_EXPORT_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "scenic/render/detail/math_alias.h"

// Historical scenic_copy TUs used bare `string` / `vector` / `map` via
// leftover types headers. Keep that sugar here so product includes need not
// pull legacy/.
using std::map;
using std::pair;
using std::string;
using std::vector;

// Export macros for leftover render DLLs:
//   scenic_impl         — scene3d / rhi3d host / Renderer2d (LEGACY_RENDER_EXPORT)
//   scenic_rhi2d_gdi      — CreateRenderDevice GDI (LEGACY_RHI2D_DEVICE_EXPORT)
//   scenic_rhi2d_gdiplus  — CreateRenderDevice GDI+ (LEGACY_RHI2D_DEVICE_EXPORT)
//   scenic_rhi2d_skia     — CreateRenderDevice Skia (LEGACY_RHI2D_DEVICE_EXPORT)
//   scenic_render_gl      — OpenGL device (LEGACY_RENDER_GL_EXPORT)
//   scenic_render_d3d     — D3D11 device (LEGACY_RENDER_D3D_EXPORT)
// Do not key these off RENDER_EXPORTS — that belongs to //src/render.

#if defined(DEM_HEIGHT_FIELD_STATIC)
#define LEGACY_RENDER_EXPORT
#elif defined(LEGACY_RENDER_EXPORTS)
#define LEGACY_RENDER_EXPORT __declspec(dllexport)
#else
#define LEGACY_RENDER_EXPORT __declspec(dllimport)
#endif

#if defined(LEGACY_RHI2D_DEVICE_EXPORTS)
#define LEGACY_RHI2D_DEVICE_EXPORT __declspec(dllexport)
#else
#define LEGACY_RHI2D_DEVICE_EXPORT
#endif

#if defined(LEGACY_RENDER_GL_EXPORTS)
#define LEGACY_RENDER_GL_EXPORT __declspec(dllexport)
#else
#define LEGACY_RENDER_GL_EXPORT __declspec(dllimport)
#endif

#if defined(LEGACY_RENDER_D3D_EXPORTS)
#define LEGACY_RENDER_D3D_EXPORT __declspec(dllexport)
#else
#define LEGACY_RENDER_D3D_EXPORT __declspec(dllimport)
#endif

#endif  // LEGACY_RENDER_LEGACY_RENDER_EXPORT_H_
