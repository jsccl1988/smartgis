// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI3D_D3D_EXT_INTERFACE_H_
#define SCENIC_RHI3D_D3D_EXT_INTERFACE_H_

#include <windows.h>

#include "scenic/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/render_device.h"

// Cross-DLL helpers exported by scenic_render_d3d. Resolved via LoadLibrary so
// scenic_impl does not link-import the D3D DLL (avoids GN cycle).
namespace scenic {
namespace detail {

using D3dCaptureBgr24Fn = long (*)(RenderDevice3d*, unsigned char*, int,
                                      int);
using D3dDrawScreenBgraFn = long (*)(RenderDevice3d*, float, float, int,
                                        int, const unsigned char*);

inline HMODULE scenic_render_d3d_module() {
  // Cache the module handle — MapLabelBatch calls DrawScreenBgra ~30×/frame.
#if defined(_DEBUG)
  static HMODULE mod = ::LoadLibraryA("scenic_render_d3d_d.dll");
#else
  static HMODULE mod = ::LoadLibraryA("scenic_render_d3d.dll");
#endif
  return mod;
}

inline long call_smt_d3d_capture_bgr24(RenderDevice3d* device,
                                       unsigned char* out_bgr24, int width_px,
                                       int height_px) {
  HMODULE mod = scenic_render_d3d_module();
  if (!mod) {
    return SMT_ERR_FAILURE;
  }
  static D3dCaptureBgr24Fn fn =
      reinterpret_cast<D3dCaptureBgr24Fn>(
          ::GetProcAddress(mod, "D3dCaptureBgr24"));
  if (!fn) {
    return SMT_ERR_FAILURE;
  }
  return fn(device, out_bgr24, width_px, height_px);
}

inline long call_smt_d3d_draw_screen_bgra(RenderDevice3d* device, float cx,
                                          float cy, int w, int h,
                                          const unsigned char* bgra) {
  HMODULE mod = scenic_render_d3d_module();
  if (!mod) {
    return SMT_ERR_FAILURE;
  }
  static D3dDrawScreenBgraFn fn =
      reinterpret_cast<D3dDrawScreenBgraFn>(
          ::GetProcAddress(mod, "D3dDrawScreenBgra"));
  if (!fn) {
    return SMT_ERR_FAILURE;
  }
  return fn(device, cx, cy, w, h, bgra);
}

using D3dDeferredEnabledFn = int (*)();
using D3dBeginDeferredDrawFn = long (*)(RenderDevice3d*, int);
using D3dBindDeferredWorkerFn = long (*)(RenderDevice3d*, int);
using D3dFinishDeferredDrawFn = long (*)(RenderDevice3d*);

inline bool smt_d3d_deferred_enabled() {
  HMODULE mod = scenic_render_d3d_module();
  if (!mod) {
    return false;
  }
  auto* fn = reinterpret_cast<D3dDeferredEnabledFn>(
      ::GetProcAddress(mod, "D3dDeferredEnabled"));
  if (!fn) {
    return false;
  }
  return fn() != 0;
}

inline long call_smt_d3d_begin_deferred(RenderDevice3d* device,
                                        int worker_count) {
  HMODULE mod = scenic_render_d3d_module();
  if (!mod) {
    return SMT_ERR_FAILURE;
  }
  auto* fn = reinterpret_cast<D3dBeginDeferredDrawFn>(
      ::GetProcAddress(mod, "D3dBeginDeferredDraw"));
  if (!fn) {
    return SMT_ERR_FAILURE;
  }
  return fn(device, worker_count);
}

inline long call_smt_d3d_bind_deferred(RenderDevice3d* device, int slot) {
  HMODULE mod = scenic_render_d3d_module();
  if (!mod) {
    return SMT_ERR_FAILURE;
  }
  auto* fn = reinterpret_cast<D3dBindDeferredWorkerFn>(
      ::GetProcAddress(mod, "D3dBindDeferredWorker"));
  if (!fn) {
    return SMT_ERR_FAILURE;
  }
  return fn(device, slot);
}

inline long call_smt_d3d_finish_deferred(RenderDevice3d* device) {
  HMODULE mod = scenic_render_d3d_module();
  if (!mod) {
    return SMT_ERR_FAILURE;
  }
  auto* fn = reinterpret_cast<D3dFinishDeferredDrawFn>(
      ::GetProcAddress(mod, "D3dFinishDeferredDraw"));
  if (!fn) {
    return SMT_ERR_FAILURE;
  }
  return fn(device);
}

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI3D_D3D_EXT_INTERFACE_H_
