// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/scenic_impl_export.h"
#include "scenic/render/rhi3d/impl/d3d/host/render_device.h"

extern "C" {

// Factory selected by Renderer3d::CreateDevice("Direct3D").
// Release lives in this DLL (scenic_render_d3d), parallel to GL.
LEGACY_RENDER_D3D_EXPORT HRESULT
CreateD3DRenderDevice(HINSTANCE hDLL, scenic::detail::RenderDevice3d*& pDevice) {
  if (!pDevice) {
    pDevice = new scenic::detail::D3dRenderDevice(hDLL);
    return SMT_OK;
  }
  return SMT_FALSE;
}

LEGACY_RENDER_D3D_EXPORT HRESULT
Release3DRenderDevice(scenic::detail::RenderDevice3d*& pDevice) {
  if (!pDevice) {
    return SMT_FALSE;
  }
  delete pDevice;
  pDevice = nullptr;
  return SMT_OK;
}

// Cross-DLL helpers: RTTI for D3dRenderDevice stays inside this module.
LEGACY_RENDER_D3D_EXPORT long D3dCaptureBgr24(
    scenic::detail::RenderDevice3d* device, unsigned char* out_bgr24, int width_px,
    int height_px) {
  auto* d3d = dynamic_cast<scenic::detail::D3dRenderDevice*>(device);
  if (!d3d || !out_bgr24 || width_px <= 0 || height_px <= 0) {
    return SMT_ERR_FAILURE;
  }
  return d3d->CaptureBgr24(out_bgr24, width_px, height_px);
}

LEGACY_RENDER_D3D_EXPORT long D3dDrawScreenBgra(
    scenic::detail::RenderDevice3d* device, float cx, float cy, int w, int h,
    const unsigned char* bgra) {
  auto* d3d = dynamic_cast<scenic::detail::D3dRenderDevice*>(device);
  if (!d3d || !bgra || w <= 0 || h <= 0) {
    return SMT_ERR_FAILURE;
  }
  return d3d->DrawScreenBgra(cx, cy, w, h, bgra);
}

LEGACY_RENDER_D3D_EXPORT int D3dDeferredEnabled(void) {
  return scenic::detail::d3d_deferred_env_enabled() ? 1 : 0;
}

LEGACY_RENDER_D3D_EXPORT long D3dBeginDeferredDraw(
    scenic::detail::RenderDevice3d* device, int worker_count) {
  auto* d3d = dynamic_cast<scenic::detail::D3dRenderDevice*>(device);
  if (!d3d) {
    return SMT_ERR_FAILURE;
  }
  return d3d->begin_deferred_draw(worker_count);
}

LEGACY_RENDER_D3D_EXPORT long D3dBindDeferredWorker(
    scenic::detail::RenderDevice3d* device, int slot) {
  auto* d3d = dynamic_cast<scenic::detail::D3dRenderDevice*>(device);
  if (!d3d) {
    return SMT_ERR_FAILURE;
  }
  return d3d->bind_deferred_worker(slot);
}

LEGACY_RENDER_D3D_EXPORT long D3dFinishDeferredDraw(
    scenic::detail::RenderDevice3d* device) {
  auto* d3d = dynamic_cast<scenic::detail::D3dRenderDevice*>(device);
  if (!d3d) {
    return SMT_ERR_FAILURE;
  }
  return d3d->finish_deferred_draw();
}

}  // extern "C"
