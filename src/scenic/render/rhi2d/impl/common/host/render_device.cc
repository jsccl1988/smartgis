// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#include "scenic/render/rhi2d/impl/common/host/render_device.h"

#include <memory>
#include <mutex>

#include "base/core/log.h"
#include "scenic/detail/err.h"
#include "scenic/render/rhi2d/impl/common/paint/backend/paint_backend.h"
#include "scenic/render/rhi2d/impl/gdiplus/aa/gdiplus.h"
#include "scenic/render/rhi2d/impl/common/cc/layer_tree_host.h"

using namespace gis;
using namespace base;

namespace scenic {
namespace detail {

int CreateRenderDevice(HINSTANCE hInst, LPRENDERDEVICE &pMrdDevice) {
  if (!pMrdDevice) {
    pMrdDevice = new Rhi2dRenderDevice(hInst);

    return SMT_ERR_NONE;
  }
  return SMT_ERR_FAILURE;
}

int DestroyRenderDevice(LPRENDERDEVICE &pMrdDevice) {
  if (!pMrdDevice) return SMT_ERR_FAILURE;

  // Release may detach a still-running worker that holds Viewport& into this
  // device. Deleting then UAFs on close �?leak until process exit instead.
  auto *gdi = static_cast<Rhi2dRenderDevice *>(pMrdDevice);
  if (gdi->release_may_leak()) {
    pMrdDevice = nullptr;
    return SMT_ERR_NONE;
  }

  SMT_SAFE_DELETE(pMrdDevice);

  return SMT_ERR_NONE;
}

Rhi2dRenderDevice::Rhi2dRenderDevice(HINSTANCE hInst)
    : RenderDevice2d(hInst),
      present_(this),
      buffer_image_(this),
      carto_draw_(std::make_unique<detail::Rhi2dCartoDraw>(hInst)),
      overlay_painter_(std::make_unique<detail::Rhi2dPainter>(
          carto_draw_.get(), &raster_back_, &map_front_, &vir_viewport1_,
          &vir_viewport2_, &shared_front_mu_)),
      layer_tree_host_(nullptr) {
  m_rBaseApi = detail::rhi2d_port_api();

  m_curDrawingOrg.x = 0;
  m_curDrawingOrg.y = 0;

  // Overlay walk only �?no FrameJob scheduler. Full map �?LayerTreeHost.
  overlay_painter_->set_scheduler(nullptr);
  overlay_painter_->set_context(&host_rc_);
  overlay_painter_->set_render_options(&m_rdOptions);

  layer_tree_host_ = new detail::Rhi2dLayerTreeHost(
      hInst, vir_viewport1_, vir_viewport2_, &shared_front_mu_);
}

Rhi2dRenderDevice::~Rhi2dRenderDevice(void) {
  if (m_leak_on_close_) {
    // Worker still references Viewport members; do not Release/delete them.
    return;
  }
  Release();
}

bool Rhi2dRenderDevice::release_may_leak() {
  Release();
  return m_leak_on_close_;
}

int Rhi2dRenderDevice::Init(HWND hWnd, const char *logname) {
  if (hWnd == nullptr || logname == nullptr) return SMT_ERR_INVALID_PARAM;

  m_hWnd = hWnd;
  (void)gdiplus_ensure_started();

  LOGGING(LOG_INFO, "Init %s RenderDevice2d ok!",
          detail::rhi2d_port_name());

  m_strLogName = logname;

  compose_buf_.set_wnd(m_hWnd);
  map_front_.set_wnd(m_hWnd);
  dynamic_buf_.set_wnd(m_hWnd);
  raster_back_.set_wnd(m_hWnd);

  layer_tree_host_->init(m_hWnd, logname);
  // Stay suspended until ReRenderMapByProxy/refresh. submit_frame here races
  // CreateNewFrame's nested pump and hangs --self-test after OnCreate.

  return SMT_ERR_NONE;
}

int Rhi2dRenderDevice::Destroy(void) {
  LOGGING(LOG_INFO, "Destroy Gdi RenderDevice2d ok!");

  return SMT_ERR_NONE;
}

int Rhi2dRenderDevice::Release(void) {
  // Stop Timer present path before tearing down shared front / HWND.
  present_.reset();

  // Stop the worker before touching GDI objects it may still reference.
  bool worker_detached = false;
  bool worker_still_running = false;
  if (layer_tree_host_) {
    worker_detached = layer_tree_host_->shutdown();
    worker_still_running = worker_detached && !layer_tree_host_->has_exited();
  }

  LOGGING(LOG_INFO, "Release Gdi RenderDevice2d ok!");

  carto_draw_->flush_style();

  // Join tile/layer resident threads on this thread (not DllMain). Skip when
  // a leaked FrameJob may still be inside run_tiles.
  if (!worker_still_running) {
    detail::rhi2d_shutdown_static_raster_runners();
  }

  // Detached worker still owns `this` briefly; deleting here UAFs on close.
  if (worker_detached) {
    layer_tree_host_ = nullptr;
    if (worker_still_running) {
      m_leak_on_close_ = true;
    }
  } else {
    SMT_SAFE_DELETE(layer_tree_host_);
  }
  // Invalidate HWND so late Refresh/Timer present cannot GetDC.
  m_hWnd = nullptr;
  return SMT_ERR_NONE;
}

int Rhi2dRenderDevice::Resize(int orgx, int orgy, int cx, int cy) {
  if (cx < 0 || cy < 0) return SMT_ERR_FAILURE;

  if (is_equal(m_Viewport.m_fVOX, orgx, dEPSILON) &&
      is_equal(m_Viewport.m_fVOY, orgy, dEPSILON) &&
      is_equal(m_Viewport.m_fVHeight, cy, dEPSILON) &&
      is_equal(m_Viewport.m_fVWidth, cx, dEPSILON)) {
    return SMT_ERR_FAILURE;
  }

  // LTH shared_buf_ is share_from(map_front_). Reallocating the host
  // published bitmap while a FrameJob still aliases the old HBITMAP leaves
  // a dangling front �?black present and fail-fast on publish (0xC0000409).
  // Wait only �?do not cancel(); sticky cancel aborted the next ZoomToRect.
  if (layer_tree_host_) {
    for (int i = 0; i < 2000 && layer_tree_host_->is_busy(); ++i) {
      ::Sleep(1);
    }
    if (layer_tree_host_->is_busy()) {
      return SMT_ERR_FAILURE;
    }
  }

  m_Viewport.m_fVOX = orgx;
  m_Viewport.m_fVOY = orgy;
  m_Viewport.m_fVHeight = cy;
  m_Viewport.m_fVWidth = cx;

  vir_viewport1_ = m_Viewport;
  vir_viewport2_ = m_Viewport;

  if (is_equal(m_Windowport.m_fWWidth, 0, dEPSILON) ||
      is_equal(m_Windowport.m_fWHeight, 0, dEPSILON)) {
    m_fblc = 1.f;
  } else {
    float xblc, yblc;
    xblc = m_Viewport.m_fVWidth / m_Windowport.m_fWWidth;
    yblc = m_Viewport.m_fVHeight / m_Windowport.m_fWHeight;
    m_fblc = (xblc > yblc) ? yblc : xblc;
  }

  // Do not HWND-present cleared buffers here �?that flashed empty frames and
  // raced OnEraseBkgnd. Timer / ZoomToRect Refresh owns the first present.
  dynamic_overlay_live_ = false;
  if (SMT_ERR_NONE ==
          compose_buf_.set_size(m_Viewport.m_fVWidth, m_Viewport.m_fVHeight) &&
      SMT_ERR_NONE ==
          dynamic_buf_.set_size(m_Viewport.m_fVWidth, m_Viewport.m_fVHeight) &&
      SMT_ERR_NONE ==
          map_front_.set_size(m_Viewport.m_fVWidth, m_Viewport.m_fVHeight) &&
      SMT_ERR_NONE ==
          raster_back_.set_size(m_Viewport.m_fVWidth, m_Viewport.m_fVHeight) &&
      SMT_ERR_NONE == layer_tree_host_->resize(orgx, orgy, cx, cy, map_front_)) {
    return SMT_ERR_NONE;
  }

  return SMT_ERR_FAILURE;
}

int Rhi2dRenderDevice::Lock() {
  lock_.lock();
  return SMT_ERR_NONE;
}

int Rhi2dRenderDevice::Unlock() {
  lock_.unlock();
  return SMT_ERR_NONE;
}

int Rhi2dRenderDevice::ScheduleDelayedRedraw(const Map *pMap) {
  return present_.schedule_delayed_redraw(pMap);
}

int Rhi2dRenderDevice::ScheduleUrgentRedraw(const Map *pMap) {
  return present_.schedule_urgent_redraw(pMap);
}

int Rhi2dRenderDevice::stage_map_job(const Map *pMap, int x, int y, int w,
                                      int h, int op, bool urgent) {
  return present_.stage_map_job(pMap, x, y, w, h, op, urgent);
}

bool Rhi2dRenderDevice::submit_staged_job() {
  return present_.submit_staged_job();
}

void Rhi2dRenderDevice::invalidate_map_present() {
  present_.invalidate_map_present();
}

int Rhi2dRenderDevice::Timer() { return present_.on_timer(); }

}  // namespace detail
}  // namespace scenic
