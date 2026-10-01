// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#include "legacy/render/rhi2d/impl/common/host/present_controller.h"

#include <mutex>

#include "legacy/render/rhi2d/impl/common/host/render_device.h"
#include "legacy/render/rhi2d/impl/common/host/preview_transform.h"
#include "legacy/render/rhi2d/impl/common/cc/layer_tree_host.h"

using namespace gis;
using namespace base;
using namespace geo;

namespace render {
namespace {
const float kDelaySec = 0.20f;
}  // namespace

Rhi2dPresentController::Rhi2dPresentController(SmtRhi2dRenderDevice* device)
    : device_(device) {}

void Rhi2dPresentController::arm_present() { present_pending_ = true; }

void Rhi2dPresentController::reset() {
  present_pending_ = false;
  redraw_pending_ = false;
  urgent_submit_ = false;
  staged_map_ = nullptr;
}

int Rhi2dPresentController::schedule_delayed_redraw(const SmtMap* pMap) {
  if (!pMap || !device_->layer_tree_host_) {
    return SMT_ERR_INVALID_PARAM;
  }
  // Interactive: coalesce + debounce; Timer is the only submitter.
  return stage_map_job(pMap, static_cast<int>(device_->m_Viewport.m_fVOX),
                       static_cast<int>(device_->m_Viewport.m_fVOY),
                       static_cast<int>(device_->m_Viewport.m_fVWidth),
                       static_cast<int>(device_->m_Viewport.m_fVHeight),
                       R2_COPYPEN,
                       /*urgent=*/false);
}

int Rhi2dPresentController::stage_map_job(const SmtMap* pMap, int x, int y, int w,
                                        int h, int op, bool urgent) {
  if (!pMap || !device_->layer_tree_host_ || w == 0 || h == 0) {
    return SMT_ERR_INVALID_PARAM;
  }

  staged_map_ = pMap;
  staged_x_ = x;
  staged_y_ = y;
  staged_w_ = w;
  staged_h_ = h;
  staged_op_ = op;

  SmtRenderContext smtRC(device_->m_Viewport, device_->m_Windowport,
                         device_->m_fblc, pMap, x, y, w, h, op);
  device_->layer_tree_host_->stage_frame(smtRC, device_->m_rdOptions);

  if (!redraw_pending_) {
    redraw_pending_ = true;
    QueryPerformanceCounter((LARGE_INTEGER*)&last_redraw_stamp_);
  }
  QueryPerformanceCounter((LARGE_INTEGER*)&last_redraw_cmd_stamp_);
  if (urgent) {
    urgent_submit_ = true;
  }
  return SMT_ERR_NONE;
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
  SmtRenderContext smtRC(device_->m_Viewport, device_->m_Windowport,
                         device_->m_fblc, staged_map_, staged_x_, staged_y_,
                         staged_w_, staged_h_, staged_op_);
  present_baseline_gen_ = device_->layer_tree_host_->published_generation();
  if (!device_->layer_tree_host_->submit_frame(smtRC, device_->m_rdOptions)) {
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
        // Worker still busy â€?keep redraw_pending_; pending context coalesces.
        // Urgent stays set so the next idle tick submits without re-debounce.
      }
    }
  }

  // Present only when the worker published a newer front generation.
  // Do not advance baseline until the worker is idle and Refresh runs â€?
  // otherwise try_present consumes the gen while Refresh early-outs on
  // is_busy(), then the idle branch clears pending and the HWND never
  // composites until a later mouse-driven Refresh.
  if (present_pending_ && device_->layer_tree_host_) {
    const uint64_t published = device_->layer_tree_host_->published_generation();
    if (published != present_baseline_gen_) {
      if (!device_->layer_tree_host_->is_busy()) {
        present_baseline_gen_ = published;
        present_pending_ = false;
        if (device_->m_hWnd && ::IsWindow(device_->m_hWnd)) {
          // New front already matches the settled windowport â€?drop the
          // interactive pan pixel offset or the map stays shifted.
          device_->SetCurDrawingOrg(lPoint(0, 0));
          // Preview zoom baseline = this settled windowport/fblc.
          device_->note_painted_preview_baseline();
          {
            std::lock_guard<std::mutex> front_lock(
                device_->shared_front_mutex());
            detail::reset_preview_viewports_identity(
                &device_->vir_viewport1_, &device_->vir_viewport2_,
                device_->m_Viewport);
          }
          // Refresh composes + InvalidateRect â†?OnDraw paint-DC blit.
          device_->Refresh();
        }
      }
      // else: published but worker not fully idle yet â€?retry next tick.
    } else if (!device_->layer_tree_host_->is_busy() && !redraw_pending_ &&
               !device_->layer_tree_host_->has_pending()) {
      // Submitted job finished without publish (cancel / superseded).
      present_pending_ = false;
    }
  }

  return SMT_ERR_NONE;
}

}  // namespace render
