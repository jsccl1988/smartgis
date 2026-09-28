// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/d3d/device/3drenderdevice.h"

namespace render {

long SmtD3DRenderDevice::BeginRender() {
  if (!device_ || !context_ || !rtv_) return SMT_ERR_FAILURE;
  context_->OMSetRenderTargets(1, &rtv_, dsv_);
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::EndRender() { return SMT_ERR_NONE; }

long SmtD3DRenderDevice::SwapBuffers() {
  if (!swapchain_) return SMT_ERR_FAILURE;
  // 0 = present immediately (vsync off); callers can tighten later via caps.
  const HRESULT hr = swapchain_->Present(0, 0);
  return SUCCEEDED(hr) ? SMT_ERR_NONE : SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::SetClearColor(const SmtColor& clr) {
  clear_color_[0] = clr.fRed;
  clear_color_[1] = clr.fGreen;
  clear_color_[2] = clr.fBlue;
  clear_color_[3] = clr.fA;
  if (state_manager_) {
    state_manager_->SetClearColorValue(clr.fRed, clr.fGreen, clr.fBlue, clr.fA);
  }
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SetDepthClearValue(float z) {
  clear_depth_ = z;
  if (state_manager_) state_manager_->SetClearDepthValue(z);
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SetStencilClearValue(ulong s) {
  clear_stencil_ = static_cast<UINT>(s);
  if (state_manager_) state_manager_->SetStencilClearValue(s);
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::Clear(ulong flags) {
  if (!context_) return SMT_ERR_FAILURE;

  if ((flags & CLR_COLOR) && rtv_) {
    context_->ClearRenderTargetView(rtv_, clear_color_);
  }
  if (dsv_ && ((flags & CLR_ZBUFFER) || (flags & CLR_STENCIL))) {
    UINT clear_flags = 0;
    if (flags & CLR_ZBUFFER) clear_flags |= D3D11_CLEAR_DEPTH;
    if (flags & CLR_STENCIL) clear_flags |= D3D11_CLEAR_STENCIL;
    context_->ClearDepthStencilView(dsv_, clear_flags, clear_depth_,
                                    static_cast<UINT8>(clear_stencil_));
  }
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SetViewport(Viewport3D& viewport) {
  m_viewPort = viewport;
  if (m_viewPort.ulHeight == 0 || m_viewPort.ulWidth == 0)
    return SMT_ERR_FAILURE;

  const UINT w = static_cast<UINT>(viewport.ulWidth);
  const UINT h = static_cast<UINT>(viewport.ulHeight);
  if (swapchain_ && (w != backbuffer_width_ || h != backbuffer_height_)) {
    if (SMT_ERR_NONE != resize_targets(w, h, /*resize_buffers=*/true))
      return SMT_ERR_FAILURE;
  }

  if (context_) {
    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = static_cast<float>(viewport.ulX);
    vp.TopLeftY = static_cast<float>(viewport.ulY);
    vp.Width = static_cast<float>(viewport.ulWidth);
    vp.Height = static_cast<float>(viewport.ulHeight);
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    context_->RSSetViewports(1, &vp);
  }
  if (state_manager_) state_manager_->SetViewportState(viewport);
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::DrawPrimitives(PrimitiveType /*type*/,
                                        SmtVertexBuffer* pVB,
                                        ulong /*baseVertex*/,
                                        ulong /*primitiveCount*/) {
  if (!pVB) return SMT_ERR_INVALID_PARAM;
  // GPU draw path needs input layout + shaders — deferred past v1 scaffolding.
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::DrawIndexedPrimitives(PrimitiveType /*type*/,
                                               SmtVertexBuffer* pVB,
                                               SmtIndexBuffer* pIB,
                                               ulong /*baseIndex*/,
                                               ulong /*primitiveCount*/) {
  if (!pVB || !pIB) return SMT_ERR_INVALID_PARAM;
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::DrawText(uint /*unID*/, float /*xpos*/, float /*ypos*/,
                                  float /*zpos*/, const SmtColor& /*color*/,
                                  const char*, ...) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::DrawText(uint /*nID*/, float /*xscreen*/,
                                  float /*yscreen*/, const SmtColor& /*color*/,
                                  const char*, ...) {
  return SMT_ERR_FAILURE;
}

long SmtD3DRenderDevice::DrawCube3D(Vector3 /*vCenter*/, float /*fWidth*/,
                                    SmtColor /*smtClr*/) {
  return SMT_ERR_FAILURE;
}

}  // namespace render
