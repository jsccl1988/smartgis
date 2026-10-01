// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/d3d/host/render_device.h"

#include "base/trace/event/process_trace.h"
#include "legacy/render/detail/frame_pipeline.h"

namespace render {

long SmtD3DRenderDevice::BeginRender() {
  BASE_TRACE_EVENT("BeginRender", "rhi3d.d3d");
  if (!device_ || !context_ || !rtv_) return SMT_ERR_FAILURE;
  context_->OMSetRenderTargets(1, &rtv_, dsv_);
  mesh_draw_state_bound_ = false;
  mesh_cb_valid_ = false;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::EndRender() {
  BASE_TRACE_EVENT("EndRender", "rhi3d.d3d");
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::SwapBuffers() {
  BASE_TRACE_EVENT("SwapBuffers", "rhi3d.d3d");
  if (!swapchain_ || !device_ || !context_ || !color_tex_)
    return SMT_ERR_FAILURE;

  // Blit offscreen color into the swapchain backbuffer for on-screen present.
  // Do NOT staging-copy every frame — that Flush+CopyResource starved drag FPS.
  // CaptureBgr24 copies from color_tex_ on demand (offscreen survives Present).
  ID3D11Texture2D* back = nullptr;
  if (SUCCEEDED(swapchain_->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                      reinterpret_cast<void**>(&back))) &&
      back) {
    context_->CopyResource(back, color_tex_);
    safe_release(back);
  }

  const HRESULT hr = swapchain_->Present(0, 0);
  detail::finish_legacy_frame_memory_sample();
  detail::log_legacy_flow("rhi3d.d3d SwapBuffers");
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
  // Match GL SetViewport: only update the rasterizer viewport. Leftover scene
  // sets temporary 120x120 viewports mid-frame; resizing color targets then
  // wiped Clear/Draw (640→120→640 every present).
  // Resize swapchain/offscreen targets only when the HWND client size changes
  // to match the requested viewport (real window resize path).
  if (swapchain_ && hwnd_ && ::IsWindow(hwnd_)) {
    RECT rc = {};
    ::GetClientRect(hwnd_, &rc);
    const UINT cw =
        static_cast<UINT>((rc.right > rc.left) ? (rc.right - rc.left) : 0);
    const UINT ch =
        static_cast<UINT>((rc.bottom > rc.top) ? (rc.bottom - rc.top) : 0);
    if (cw > 0 && ch > 0 && w == cw && h == ch &&
        (w != backbuffer_width_ || h != backbuffer_height_)) {
      if (SMT_ERR_NONE != resize_targets(w, h, /*resize_buffers=*/true))
        return SMT_ERR_FAILURE;
    }
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

}  // namespace render
