// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#include "legacy/render/rhi2d/impl/gdi/host/ui_controller.h"

#include <math.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "base/core/log.h"
#include "base/memory/arena.h"
#include "gis/datasource/provider/impl/ogr/codec/ogr_feature_codec.h"
#include "legacy/render/rhi2d/impl/gdi/host/render_device.h"
#include "legacy/render/rhi2d/impl/gdi/worker/render_thread.h"

using namespace gis;
using namespace base;
using namespace geo;

namespace render {
const float C_fDELAY = 0.20;

GdiUiController::GdiUiController(SmtGdiRenderDevice *device)
    : device_(device) {}

int GdiUiController::schedule_delayed_redraw(const SmtMap *pMap) {
  if (!pMap || !device_->render_thread_) {
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

int GdiUiController::stage_map_job(const SmtMap *pMap, int x, int y, int w,
                                   int h, int op, bool urgent) {
  if (!pMap || !device_->render_thread_ || w == 0 || h == 0) {
    return SMT_ERR_INVALID_PARAM;
  }

  m_staged_map = pMap;
  m_staged_x = x;
  m_staged_y = y;
  m_staged_w = w;
  m_staged_h = h;
  m_staged_op = op;

  SmtRenderContex smtRC(device_->m_Viewport, device_->m_Windowport,
                        device_->m_fblc, pMap, x, y, w, h, op);
  device_->render_thread_->stage_frame(smtRC, device_->m_rdPra);

  if (!m_bRedraw) {
    m_bRedraw = true;
    QueryPerformanceCounter((LARGE_INTEGER *)&m_llLastRedrawStamp);
  }
  QueryPerformanceCounter((LARGE_INTEGER *)&m_llLastRedrawCmdStamp);
  if (urgent) {
    m_bUrgentSubmit = true;
  }
  return SMT_ERR_NONE;
}

bool GdiUiController::submit_staged_job() {
  if (!device_->render_thread_ || !m_staged_map) {
    return false;
  }
  if (device_->render_thread_->is_busy()) {
    return false;
  }

  // Rebuild context from the live device viewport so the latest pan/zoom
  // after debounce wins over a stale stage_frame snapshot.
  SmtRenderContex smtRC(device_->m_Viewport, device_->m_Windowport,
                        device_->m_fblc, m_staged_map, m_staged_x, m_staged_y,
                        m_staged_w, m_staged_h, m_staged_op);
  m_present_baseline_gen = device_->render_thread_->published_generation();
  if (!device_->render_thread_->submit_frame(smtRC, device_->m_rdPra)) {
    return false;
  }
  m_bPresentPending = true;
  m_bRedraw = false;
  m_bUrgentSubmit = false;
  return true;
}

void GdiUiController::invalidate_map_present() {
  if (!device_->m_hWnd) {
    return;
  }
  RECT rt;
  GetClientRect(device_->m_hWnd, &rt);
  InvalidateRect(device_->m_hWnd, &rt, FALSE);
}

int GdiUiController::on_timer() {
  LONGLONG llStamp = 0, llPerCount = 0;
  QueryPerformanceFrequency((LARGE_INTEGER *)&llPerCount);
  QueryPerformanceCounter((LARGE_INTEGER *)&llStamp);

  if (m_bRedraw && device_->render_thread_) {
    const double dbfElapse =
        (llStamp - m_llLastRedrawCmdStamp) / (double)llPerCount;
    const bool due = m_bUrgentSubmit || dbfElapse > C_fDELAY;
    if (due) {
      if (!submit_staged_job()) {
        // Worker still busy - keep m_bRedraw; pending contex coalesces.
        // Urgent stays set so the next idle tick submits without re-debounce.
      }
    }
  }

  // Present only when the worker published a newer front generation.
  // Do not advance baseline until the worker is idle and Refresh runs -
  // otherwise try_present consumes the gen while Refresh early-outs on
  // is_busy(), then the idle branch clears pending and the HWND never
  // composites until a later mouse-driven Refresh.
  if (m_bPresentPending && device_->render_thread_) {
    const uint64_t published = device_->render_thread_->published_generation();
    if (published != m_present_baseline_gen) {
      if (!device_->render_thread_->is_busy()) {
        m_present_baseline_gen = published;
        m_bPresentPending = false;
        if (device_->m_hWnd && ::IsWindow(device_->m_hWnd)) {
          // New front already matches the settled windowport — drop the
          // interactive pan pixel offset or the map stays shifted.
          device_->SetCurDrawingOrg(lPoint(0, 0));
          // Preview zoom baseline = this settled windowport/fblc.
          device_->note_painted_preview_baseline();
          {
            std::lock_guard<std::mutex> front_lock(
                device_->shared_front_mutex());
            device_->vir_viewport1_ = device_->m_Viewport;
            device_->vir_viewport2_ = device_->m_Viewport;
          }
          // Refresh composes + InvalidateRect → OnDraw paint-DC blit.
          device_->Refresh();
        }
      }
      // else: published but worker not fully idle yet - retry next tick.
    } else if (!device_->render_thread_->is_busy() && !m_bRedraw &&
               !device_->render_thread_->has_pending()) {
      // Submitted job finished without publish (cancel / superseded).
      m_bPresentPending = false;
    }
  }

  return SMT_ERR_NONE;
}

}  // namespace render
