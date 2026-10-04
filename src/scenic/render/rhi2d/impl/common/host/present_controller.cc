// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#include "scenic/render/rhi2d/impl/common/host/present_controller.h"

#include <mutex>

#include "scenic/render/rhi2d/impl/common/host/render_device.h"
#include "scenic/render/rhi2d/impl/common/host/preview_transform.h"
#include "scenic/render/rhi2d/impl/common/cc/layer_tree_host.h"

using namespace gis;
using namespace base;

namespace scenic {
namespace detail {
namespace {
// Interactive pan/wheel coalesce window. 200 ms felt sticky; ~60 ms keeps
// stretch preview dominant while still merging mouse-move storms.
const float kDelaySec = 0.06f;
}  // namespace

void Rhi2dPresentController::finish_interactive_settle() {
  if (!device_->m_hWnd || !::IsWindow(device_->m_hWnd)) {
    return;
  }
  // Mid-stroke pan publish: windowport not yet committed � keep drawing-org so
  // BitBlt still follows the pointer.
  // Gesture-end: only clear org once a FrameJob *newer than the commit* lands.
  // A stale in-flight job at wp0 that settles first used to clear org (snap
  // back) then the urgent pan job settled again (second jump).
  const lPoint org = device_->GetCurDrawingOrg();
  const bool pan_slide = (org.x != 0 || org.y != 0);
  bool clear_org = !pan_slide;
  if (device_->peek_clear_drawing_org_on_publish()) {
    const uint64_t published = device_->map_published_generation();
    if (published > device_->clear_drawing_org_min_gen()) {
      clear_org = true;
      device_->clear_drawing_org_publish_request();
    } else {
      clear_org = false;
    }
  }
  if (clear_org) {
    device_->SetCurDrawingOrg(lPoint(0, 0));
  }
  device_->note_painted_preview_baseline();
  // Always drop StretchBlt leftovers. A published front already matches the
  // live windowport; composing stretch + org shears cartography (map shatter).
  {
    std::lock_guard<std::mutex> front_lock(device_->shared_front_mutex());
    detail::reset_preview_viewports_identity(&device_->vir_viewport1_,
                                             &device_->vir_viewport2_,
                                             device_->m_Viewport);
  }
  // Sync compose+BitBlt; InvalidateRect-only Refresh can defer past browse end.
  (void)device_->RenderMap();
  (void)device_->Refresh();
}

Rhi2dPresentController::Rhi2dPresentController(Rhi2dRenderDevice* device)
    : device_(device) {}

void Rhi2dPresentController::arm_present() {
  present_pending_ = true;
  if (device_->layer_tree_host_) {
    // Snapshot so Timer waits for a newer publish, or composites on idle when
    // Refresh armed present while the worker was still busy on this gen.
    present_baseline_gen_ =
        device_->layer_tree_host_->published_generation();
  }
}

void Rhi2dPresentController::reset() {
  present_pending_ = false;
  redraw_pending_ = false;
  urgent_submit_ = false;
  staged_map_ = nullptr;
}

int Rhi2dPresentController::schedule_delayed_redraw(const Map* pMap) {
  if (!pMap || !device_->layer_tree_host_) {
    return kErrInvalidParam;
  }
  // Interactive: coalesce + debounce; Timer is the only submitter.
  return stage_map_job(pMap, static_cast<int>(device_->m_Viewport.m_fVOX),
                       static_cast<int>(device_->m_Viewport.m_fVOY),
                       static_cast<int>(device_->m_Viewport.m_fVWidth),
                       static_cast<int>(device_->m_Viewport.m_fVHeight),
                       R2_COPYPEN,
                       /*urgent=*/false);
}

int Rhi2dPresentController::schedule_urgent_redraw(const Map* pMap) {
  if (!pMap || !device_->layer_tree_host_) {
    return kErrInvalidParam;
  }
  return stage_map_job(pMap, static_cast<int>(device_->m_Viewport.m_fVOX),
                       static_cast<int>(device_->m_Viewport.m_fVOY),
                       static_cast<int>(device_->m_Viewport.m_fVWidth),
                       static_cast<int>(device_->m_Viewport.m_fVHeight),
                       R2_COPYPEN,
                       /*urgent=*/true);
}

int Rhi2dPresentController::stage_map_job(const Map* pMap, int x, int y, int w,
                                        int h, int op, bool urgent) {
  if (!pMap || !device_->layer_tree_host_ || w == 0 || h == 0) {
    return kErrInvalidParam;
  }

  staged_map_ = pMap;
  staged_x_ = x;
  staged_y_ = y;
  staged_w_ = w;
  staged_h_ = h;
  staged_op_ = op;

  RenderContext rc(device_->m_Viewport, device_->m_Windowport,
                   device_->m_fblc, pMap, x, y, w, h, op);
  device_->layer_tree_host_->stage_frame(rc, device_->m_rdOptions);

  if (!redraw_pending_) {
    redraw_pending_ = true;
    QueryPerformanceCounter((LARGE_INTEGER*)&last_redraw_stamp_);
  }
  QueryPerformanceCounter((LARGE_INTEGER*)&last_redraw_cmd_stamp_);
  if (urgent) {
    urgent_submit_ = true;
  }
  return kErrNone;
}

bool Rhi2dPresentController::submit_staged_job() {
  if (!device_->layer_tree_host_ || !staged_map_) {
    return false;
  }
  if (device_->layer_tree_host_->is_busy()) {
    return false;
  }

  // Rebuild context from the live device viewport so the latest pan/zoom
  // after debounce wins over a stale stage_frame snapshot.
  RenderContext rc(device_->m_Viewport, device_->m_Windowport, device_->m_fblc,
                   staged_map_, staged_x_, staged_y_, staged_w_, staged_h_,
                   staged_op_);
  present_baseline_gen_ = device_->layer_tree_host_->published_generation();
  if (!device_->layer_tree_host_->submit_frame(rc, device_->m_rdOptions)) {
    return false;
  }
  present_pending_ = true;
  redraw_pending_ = false;
  urgent_submit_ = false;
  return true;
}

void Rhi2dPresentController::invalidate_map_present() {
  if (!device_->m_hWnd) {
    return;
  }
  RECT rt;
  GetClientRect(device_->m_hWnd, &rt);
  InvalidateRect(device_->m_hWnd, &rt, FALSE);
}

int Rhi2dPresentController::on_timer() {
  LONGLONG llStamp = 0, llPerCount = 0;
  QueryPerformanceFrequency((LARGE_INTEGER*)&llPerCount);
  QueryPerformanceCounter((LARGE_INTEGER*)&llStamp);

  if (redraw_pending_ && device_->layer_tree_host_) {
    const double dbfElapse =
        (llStamp - last_redraw_cmd_stamp_) / (double)llPerCount;
    const bool due = urgent_submit_ || dbfElapse > kDelaySec;
    if (due) {
      if (!submit_staged_job()) {
        // Worker still busy; keep redraw_pending_. Urgent stays set so the
        // next idle tick submits without re-debounce.
      }
    }
  }

  // Present only when the worker published a newer front generation.
  if (present_pending_ && device_->layer_tree_host_) {
    const uint64_t published = device_->layer_tree_host_->published_generation();
    if (published != present_baseline_gen_) {
      if (!device_->layer_tree_host_->is_busy()) {
        present_baseline_gen_ = published;
        present_pending_ = false;
        finish_interactive_settle();
      }
    } else if (!device_->layer_tree_host_->is_busy() && !redraw_pending_ &&
               !device_->layer_tree_host_->has_pending()) {
      // Refresh armed present while busy on this gen, or job finished without
      // a newer publish � still composite so browse end is not stuck.
      present_pending_ = false;
      finish_interactive_settle();
    }
  }

  return kErrNone;
}

}  // namespace detail
}  // namespace scenic
