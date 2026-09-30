// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef LEGACY_RENDER_GDI_UI_CONTROLLER_H_
#define LEGACY_RENDER_GDI_UI_CONTROLLER_H_

#include <cstdint>

#include "gis/model/map/map.h"
#include "legacy/core/macros/macros.h"

namespace render {

class SmtGdiRenderDevice;

// Stages map FrameJobs, debounces submit via Timer, and arms present-on-gen.
class GdiUiController {
 public:
  explicit GdiUiController(SmtGdiRenderDevice* device);

  int stage_map_job(const gis::SmtMap* pMap, int x, int y, int w, int h, int op,
                    bool urgent);
  bool submit_staged_job();
  void invalidate_map_present();
  int schedule_delayed_redraw(const gis::SmtMap* pMap);
  int on_timer();

  // Owned schedule / present-on-gen state (device Refresh may arm present).
  LONGLONG m_llLastRedrawCmdStamp = 0;
  LONGLONG m_llLastRedrawStamp = 0;
  bool m_bRedraw = false;
  bool m_bUrgentSubmit = false;
  bool m_bPresentPending = false;
  uint64_t m_present_baseline_gen = 0;
  const gis::SmtMap* m_staged_map = nullptr;
  int m_staged_x = 0;
  int m_staged_y = 0;
  int m_staged_w = 0;
  int m_staged_h = 0;
  int m_staged_op = R2_COPYPEN;

 private:
  SmtGdiRenderDevice* device_;
};

}  // namespace render

#endif  // LEGACY_RENDER_GDI_UI_CONTROLLER_H_
