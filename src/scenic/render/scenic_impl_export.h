// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RENDER_SCENIC_IMPL_EXPORT_H_
#define SCENIC_RENDER_SCENIC_IMPL_EXPORT_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

// Copy TUs historically used bare `string` / `vector` / `map` and integer
// aliases. Keep that sugar here so they need not pull leftover types headers.
// Scene math is `base/math/math.h` (`::base::Vector3`, `::base::fPoint`, …).
using std::map;
using std::pair;
using std::string;
using std::vector;

using uchar = unsigned char;
using ushort = unsigned short;
using ulong = unsigned long;
using uint = unsigned int;
using byte = unsigned char;

#ifndef TEMP_BUFFER_SIZE
#define TEMP_BUFFER_SIZE 255
#endif

// Export macros for Scenic copy DLLs:
//   scenic_impl         — scene3d / rhi3d host / Renderer2d (SCENIC_IMPL_EXPORT)
//   scenic_rhi2d_gdi      — CreateRenderDevice GDI (SCENIC_RHI2D_DEVICE_EXPORT)
//   scenic_rhi2d_gdiplus  — CreateRenderDevice GDI+ (SCENIC_RHI2D_DEVICE_EXPORT)
//   scenic_rhi2d_skia     — CreateRenderDevice Skia (SCENIC_RHI2D_DEVICE_EXPORT)
//   scenic_render_gl      — OpenGL device (SCENIC_RENDER_GL_EXPORT)
//   scenic_render_d3d     — D3D11 device (SCENIC_RENDER_D3D_EXPORT)
// Do not key these off RENDER_EXPORTS — that belongs to //src/render.
// Do not key these off SCENIC_EXPORTS — that belongs to scenic.dll.

#if defined(DEM_HEIGHT_FIELD_STATIC)
#define SCENIC_IMPL_EXPORT
#elif defined(SCENIC_IMPL_EXPORTS)
#define SCENIC_IMPL_EXPORT __declspec(dllexport)
#else
#define SCENIC_IMPL_EXPORT __declspec(dllimport)
#endif

#if defined(SCENIC_RHI2D_DEVICE_EXPORTS)
#define SCENIC_RHI2D_DEVICE_EXPORT __declspec(dllexport)
#else
#define SCENIC_RHI2D_DEVICE_EXPORT
#endif

#if defined(SCENIC_RENDER_GL_EXPORTS)
#define SCENIC_RENDER_GL_EXPORT __declspec(dllexport)
#else
#define SCENIC_RENDER_GL_EXPORT __declspec(dllimport)
#endif

#if defined(SCENIC_RENDER_D3D_EXPORTS)
#define SCENIC_RENDER_D3D_EXPORT __declspec(dllexport)
#else
#define SCENIC_RENDER_D3D_EXPORT __declspec(dllimport)
#endif

#endif  // SCENIC_RENDER_SCENIC_IMPL_EXPORT_H_
