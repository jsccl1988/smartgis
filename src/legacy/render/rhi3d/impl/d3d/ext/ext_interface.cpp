// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/impl/d3d/host/render_device.h"

extern "C" {

// Factory selected by Smt3DRenderer::CreateDevice("Direct3D").
// Release lives in this DLL (legacy_render_d3d), parallel to GL.
LEGACY_RENDER_D3D_EXPORT HRESULT
CreateD3DRenderDevice(HINSTANCE hDLL, render::Smt3DRenderDevice*& pDevice) {
  if (!pDevice) {
    pDevice = new render::SmtD3DRenderDevice(hDLL);
    return SMT_OK;
  }
  return SMT_FALSE;
}

LEGACY_RENDER_D3D_EXPORT HRESULT
Release3DRenderDevice(render::Smt3DRenderDevice*& pDevice) {
  if (!pDevice) {
    return SMT_FALSE;
  }
  delete pDevice;
  pDevice = nullptr;
  return SMT_OK;
}

// Cross-DLL helpers: RTTI for SmtD3DRenderDevice stays inside this module.
LEGACY_RENDER_D3D_EXPORT long SmtD3DCaptureBgr24(
    render::Smt3DRenderDevice* device, unsigned char* out_bgr24, int width_px,
    int height_px) {
  auto* d3d = dynamic_cast<render::SmtD3DRenderDevice*>(device);
  if (!d3d || !out_bgr24 || width_px <= 0 || height_px <= 0) {
    return SMT_ERR_FAILURE;
  }
  return d3d->CaptureBgr24(out_bgr24, width_px, height_px);
}

LEGACY_RENDER_D3D_EXPORT long SmtD3DDrawScreenBgra(
    render::Smt3DRenderDevice* device, float cx, float cy, int w, int h,
    const unsigned char* bgra) {
  auto* d3d = dynamic_cast<render::SmtD3DRenderDevice*>(device);
  if (!d3d || !bgra || w <= 0 || h <= 0) {
    return SMT_ERR_FAILURE;
  }
  return d3d->DrawScreenBgra(cx, cy, w, h, bgra);
}

LEGACY_RENDER_D3D_EXPORT int SmtD3DDeferredEnabled(void) {
  return render::d3d_deferred_env_enabled() ? 1 : 0;
}

LEGACY_RENDER_D3D_EXPORT long SmtD3DBeginDeferredDraw(
    render::Smt3DRenderDevice* device, int worker_count) {
  auto* d3d = dynamic_cast<render::SmtD3DRenderDevice*>(device);
  if (!d3d) {
    return SMT_ERR_FAILURE;
  }
  return d3d->begin_deferred_draw(worker_count);
}

LEGACY_RENDER_D3D_EXPORT long SmtD3DBindDeferredWorker(
    render::Smt3DRenderDevice* device, int slot) {
  auto* d3d = dynamic_cast<render::SmtD3DRenderDevice*>(device);
  if (!d3d) {
    return SMT_ERR_FAILURE;
  }
  return d3d->bind_deferred_worker(slot);
}

LEGACY_RENDER_D3D_EXPORT long SmtD3DFinishDeferredDraw(
    render::Smt3DRenderDevice* device) {
  auto* d3d = dynamic_cast<render::SmtD3DRenderDevice*>(device);
  if (!d3d) {
    return SMT_ERR_FAILURE;
  }
  return d3d->finish_deferred_draw();
}

}  // extern "C"
