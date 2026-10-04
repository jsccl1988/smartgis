// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi3d/impl/d3d/host/deferred_draw.h"

#include <cstdlib>

#include "base/trace/event/process_trace.h"
#include "base/process/switches.h"
#include "scenic/render/rhi3d/impl/d3d/host/render_device.h"

namespace scenic {
namespace detail {
namespace {

thread_local D3dRenderDevice* g_tls_deferred_device = nullptr;
thread_local int g_tls_deferred_slot = -1;

}  // namespace

bool d3d_deferred_env_enabled() {
  const char* e = base::switch_cstr("rhi3d-d3d-deferred");
  if (!e || !e[0]) {
    return true;
  }
  return !(e[0] == '0' || e[0] == 'n' || e[0] == 'N' || e[0] == 'f' ||
           e[0] == 'F');
}

ID3D11DeviceContext* D3dRenderDevice::active_context() const {
  if (g_tls_deferred_device == this && g_tls_deferred_slot >= 0 &&
      g_tls_deferred_slot < static_cast<int>(deferred_slots_.size()) &&
      deferred_slots_[static_cast<size_t>(g_tls_deferred_slot)].ctx) {
    return deferred_slots_[static_cast<size_t>(g_tls_deferred_slot)].ctx;
  }
  return context_;
}

ID3D11Buffer* D3dRenderDevice::active_mesh_cb() const {
  if (g_tls_deferred_device == this && g_tls_deferred_slot >= 0 &&
      g_tls_deferred_slot < static_cast<int>(deferred_slots_.size()) &&
      deferred_slots_[static_cast<size_t>(g_tls_deferred_slot)].mesh_cb) {
    return deferred_slots_[static_cast<size_t>(g_tls_deferred_slot)].mesh_cb;
  }
  return mesh_cb_;
}

bool D3dRenderDevice::deferred_draw_active() const {
  return deferred_recording_;
}

long D3dRenderDevice::begin_deferred_draw(int worker_count) {
  BASE_TRACE_EVENT("begin_deferred", "rhi3d.d3d.deferred");
  if (!d3d_deferred_env_enabled() || !device_ || !context_ || !rtv_) {
    return kErrFailure;
  }
  if (ensure_mesh_pipeline() != kErrNone || !mesh_cb_) {
    return kErrFailure;
  }
  if (worker_count < 1) {
    worker_count = 1;
  }
  if (worker_count > 8) {
    worker_count = 8;
  }

  // Grow / create deferred slots.
  while (static_cast<int>(deferred_slots_.size()) < worker_count) {
    D3dDeferredSlot slot;
    ID3D11DeviceContext* def = nullptr;
    const HRESULT hr = device_->CreateDeferredContext(0, &def);
    if (FAILED(hr) || !def) {
      return kErrFailure;
    }
    slot.ctx = def;
    if (mesh_cb_) {
      D3D11_BUFFER_DESC desc = {};
      mesh_cb_->GetDesc(&desc);
      ID3D11Buffer* cb = nullptr;
      if (SUCCEEDED(device_->CreateBuffer(&desc, nullptr, &cb)) && cb) {
        slot.mesh_cb = cb;
      }
    }
    if (!slot.mesh_cb) {
      safe_release(slot.ctx);
      return kErrFailure;
    }
    deferred_slots_.push_back(slot);
  }

  for (D3dDeferredSlot& s : deferred_slots_) {
    safe_release(s.list);
  }
  deferred_recording_ = true;
  g_tls_deferred_device = nullptr;
  g_tls_deferred_slot = -1;
  return kErrNone;
}

long D3dRenderDevice::bind_deferred_worker(int slot) {
  if (slot < 0) {
    g_tls_deferred_device = nullptr;
    g_tls_deferred_slot = -1;
    return kErrNone;
  }
  if (!deferred_recording_ || slot >= static_cast<int>(deferred_slots_.size()) ||
      !deferred_slots_[static_cast<size_t>(slot)].ctx) {
    return kErrFailure;
  }
  g_tls_deferred_device = this;
  g_tls_deferred_slot = slot;
  ID3D11DeviceContext* ctx = deferred_slots_[static_cast<size_t>(slot)].ctx;
  ctx->OMSetRenderTargets(1, &rtv_, dsv_);
  D3D11_VIEWPORT vp = {};
  vp.TopLeftX = static_cast<float>(m_viewPort.ulX);
  vp.TopLeftY = static_cast<float>(m_viewPort.ulY);
  vp.Width = static_cast<float>(m_viewPort.ulWidth);
  vp.Height = static_cast<float>(m_viewPort.ulHeight);
  vp.MinDepth = 0.f;
  vp.MaxDepth = 1.f;
  ctx->RSSetViewports(1, &vp);
  return kErrNone;
}

long D3dRenderDevice::finish_deferred_draw() {
  BASE_TRACE_EVENT("finish_deferred", "rhi3d.d3d.deferred");
  g_tls_deferred_device = nullptr;
  g_tls_deferred_slot = -1;
  if (!deferred_recording_ || !context_) {
    deferred_recording_ = false;
    return kErrFailure;
  }

  for (D3dDeferredSlot& s : deferred_slots_) {
    if (!s.ctx) {
      continue;
    }
    safe_release(s.list);
    ID3D11CommandList* list = nullptr;
    if (FAILED(s.ctx->FinishCommandList(FALSE, &list)) || !list) {
      deferred_recording_ = false;
      return kErrFailure;
    }
    s.list = list;
  }

  for (D3dDeferredSlot& s : deferred_slots_) {
    if (!s.list) {
      continue;
    }
    context_->ExecuteCommandList(s.list, FALSE);
    safe_release(s.list);
  }

  // Restore immediate OM + viewport after ExecuteCommandList (clears state).
  // Without RSSetViewports, subsequent DrawScreenBgra (MapLabelBatch) can clip
  // to a degenerate default viewport and leave only a stray coastal label.
  if (rtv_) {
    context_->OMSetRenderTargets(1, &rtv_, dsv_);
  }
  {
    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = static_cast<float>(m_viewPort.ulX);
    vp.TopLeftY = static_cast<float>(m_viewPort.ulY);
    vp.Width = static_cast<float>(
        m_viewPort.ulWidth > 0 ? m_viewPort.ulWidth : backbuffer_width_);
    vp.Height = static_cast<float>(
        m_viewPort.ulHeight > 0 ? m_viewPort.ulHeight : backbuffer_height_);
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    if (vp.Width > 0.f && vp.Height > 0.f) {
      context_->RSSetViewports(1, &vp);
    }
  }
  deferred_recording_ = false;
  return kErrNone;
}

}  // namespace detail
}  // namespace scenic
