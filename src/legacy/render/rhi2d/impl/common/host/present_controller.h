// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef LEGACY_RENDER_GDI_PRESENT_CONTROLLER_H_
#define LEGACY_RENDER_GDI_PRESENT_CONTROLLER_H_

#include <cstdint>

#include "gis/model/map/map.h"
#include "legacy/core/macros/macros.h"

namespace render {

class SmtRhi2dRenderDevice;

// Host-side input coalesce + present-on-gen gate (debounce via Timer).
// Frame beat stays on LayerTreeHost / Scheduler — this type only stages,
// submits when due, and arms HWND present after a newer published gen.
class Rhi2dPresentController {
 public:
  explicit Rhi2dPresentController(SmtRhi2dRenderDevice* device);

  int stage_map_job(const gis::SmtMap* pMap, int x, int y, int w, int h, int op,
                    bool urgent);
  bool submit_staged_job();
  void invalidate_map_present();
  int schedule_delayed_redraw(const gis::SmtMap* pMap);
  int schedule_urgent_redraw(const gis::SmtMap* pMap);
  int on_timer();

  // Arm present-on-gen (e.g. Refresh while the worker is still busy).
  void arm_present();
  // Clear debounce / present state (Release / teardown).
  void reset();

 private:
  void finish_interactive_settle();

  SmtRhi2dRenderDevice* device_;

  LONGLONG last_redraw_cmd_stamp_ = 0;
  LONGLONG last_redraw_stamp_ = 0;
  bool redraw_pending_ = false;
  bool urgent_submit_ = false;
  bool present_pending_ = false;
  uint64_t present_baseline_gen_ = 0;
  const gis::SmtMap* staged_map_ = nullptr;
  int staged_x_ = 0;
  int staged_y_ = 0;
  int staged_w_ = 0;
  int staged_h_ = 0;
  int staged_op_ = R2_COPYPEN;
};

}  // namespace render

#endif  // LEGACY_RENDER_GDI_PRESENT_CONTROLLER_H_
