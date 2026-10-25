// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_GPU_DAMAGE_MAP2D_DAMAGE_RECT_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_GPU_DAMAGE_MAP2D_DAMAGE_RECT_H_

#include <cstdint>

#include "content/browser/present/map2d/frame/cache/map2d_frame_cache.h"

namespace content {

class GisScene;
class ViewFrame;

// Pixel damage for T1.1 local scissor (not a Chromium layer tree).
struct DamageRect {
  int32_t x = 0;
  int32_t y = 0;
  uint32_t w = 0;
  uint32_t h = 0;
  bool empty() const { return w == 0 || h == 0; }
};

void union_damage_rect(DamageRect* acc, const DamageRect& add);
void clamp_damage_rect(DamageRect* rect, uint32_t width_px, uint32_t height_px);

// True when |reasons| is a non-empty subset of selection/shell overlay only
// and a non-empty |out| rect is computed (shell-only returns false — no
// scissor so map underlay can StaticReuse).
bool try_local_damage_rect(uint32_t reasons, const GisScene* scene,
                           const ViewFrame* frame,
                           const Map2dFrameCache::CameraKey& cam,
                           bool flash_pulse, DamageRect* last_selection_rect,
                           DamageRect* out);

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_GPU_DAMAGE_MAP2D_DAMAGE_RECT_H_
