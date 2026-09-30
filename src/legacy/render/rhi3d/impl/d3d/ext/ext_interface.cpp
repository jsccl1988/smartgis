// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/impl/d3d/host/render_device.h"

extern "C" {

// Factory selected by Smt3DRenderer::CreateDevice("Direct3D").
// Release reuses the shared Release3DRenderDevice export from the GL module
// (same legacy_render DLL; deletes any Smt3DRenderDevice*).
LEGACY_RENDER_EXPORT HRESULT
CreateD3DRenderDevice(HINSTANCE hDLL, render::Smt3DRenderDevice*& pDevice) {
  if (!pDevice) {
    pDevice = new render::SmtD3DRenderDevice(hDLL);
    return SMT_OK;
  }
  return SMT_FALSE;
}

}  // extern "C"
