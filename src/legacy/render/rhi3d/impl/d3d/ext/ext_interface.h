// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_RHI3D_D3D_EXT_INTERFACE_H_
#define SMT_LEGACY_RENDER_RHI3D_D3D_EXT_INTERFACE_H_

#include <windows.h>

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/render_device.h"

// Cross-DLL helpers exported by legacy_render_d3d. Resolved via LoadLibrary so
// legacy_render does not link-import the D3D DLL (avoids GN cycle).
namespace render {
namespace detail {

using SmtD3DCaptureBgr24Fn = long (*)(Smt3DRenderDevice*, unsigned char*, int,
                                      int);
using SmtD3DDrawScreenBgraFn = long (*)(Smt3DRenderDevice*, float, float, int,
                                        int, const unsigned char*);

inline HMODULE legacy_render_d3d_module() {
  // Cache the module handle — MapLabelBatch calls DrawScreenBgra ~30×/frame.
#if defined(_DEBUG)
  static HMODULE mod = ::LoadLibraryA("legacy_render_d3d_d.dll");
#else
  static HMODULE mod = ::LoadLibraryA("legacy_render_d3d.dll");
#endif
  return mod;
}

inline long call_smt_d3d_capture_bgr24(Smt3DRenderDevice* device,
                                       unsigned char* out_bgr24, int width_px,
                                       int height_px) {
  HMODULE mod = legacy_render_d3d_module();
  if (!mod) {
    return SMT_ERR_FAILURE;
  }
  static SmtD3DCaptureBgr24Fn fn =
      reinterpret_cast<SmtD3DCaptureBgr24Fn>(
          ::GetProcAddress(mod, "SmtD3DCaptureBgr24"));
  if (!fn) {
    return SMT_ERR_FAILURE;
  }
  return fn(device, out_bgr24, width_px, height_px);
}

inline long call_smt_d3d_draw_screen_bgra(Smt3DRenderDevice* device, float cx,
                                          float cy, int w, int h,
                                          const unsigned char* bgra) {
  HMODULE mod = legacy_render_d3d_module();
  if (!mod) {
    return SMT_ERR_FAILURE;
  }
  static SmtD3DDrawScreenBgraFn fn =
      reinterpret_cast<SmtD3DDrawScreenBgraFn>(
          ::GetProcAddress(mod, "SmtD3DDrawScreenBgra"));
  if (!fn) {
    return SMT_ERR_FAILURE;
  }
  return fn(device, cx, cy, w, h, bgra);
}

using SmtD3DDeferredEnabledFn = int (*)();
using SmtD3DBeginDeferredDrawFn = long (*)(Smt3DRenderDevice*, int);
using SmtD3DBindDeferredWorkerFn = long (*)(Smt3DRenderDevice*, int);
using SmtD3DFinishDeferredDrawFn = long (*)(Smt3DRenderDevice*);

inline bool smt_d3d_deferred_enabled() {
  HMODULE mod = legacy_render_d3d_module();
  if (!mod) {
    return false;
  }
  auto* fn = reinterpret_cast<SmtD3DDeferredEnabledFn>(
      ::GetProcAddress(mod, "SmtD3DDeferredEnabled"));
  if (!fn) {
    return false;
  }
  return fn() != 0;
}

inline long call_smt_d3d_begin_deferred(Smt3DRenderDevice* device,
                                        int worker_count) {
  HMODULE mod = legacy_render_d3d_module();
  if (!mod) {
    return SMT_ERR_FAILURE;
  }
  auto* fn = reinterpret_cast<SmtD3DBeginDeferredDrawFn>(
      ::GetProcAddress(mod, "SmtD3DBeginDeferredDraw"));
  if (!fn) {
    return SMT_ERR_FAILURE;
  }
  return fn(device, worker_count);
}

inline long call_smt_d3d_bind_deferred(Smt3DRenderDevice* device, int slot) {
  HMODULE mod = legacy_render_d3d_module();
  if (!mod) {
    return SMT_ERR_FAILURE;
  }
  auto* fn = reinterpret_cast<SmtD3DBindDeferredWorkerFn>(
      ::GetProcAddress(mod, "SmtD3DBindDeferredWorker"));
  if (!fn) {
    return SMT_ERR_FAILURE;
  }
  return fn(device, slot);
}

inline long call_smt_d3d_finish_deferred(Smt3DRenderDevice* device) {
  HMODULE mod = legacy_render_d3d_module();
  if (!mod) {
    return SMT_ERR_FAILURE;
  }
  auto* fn = reinterpret_cast<SmtD3DFinishDeferredDrawFn>(
      ::GetProcAddress(mod, "SmtD3DFinishDeferredDraw"));
  if (!fn) {
    return SMT_ERR_FAILURE;
  }
  return fn(device);
}

}  // namespace detail
}  // namespace render

#endif  // SMT_LEGACY_RENDER_RHI3D_D3D_EXT_INTERFACE_H_
