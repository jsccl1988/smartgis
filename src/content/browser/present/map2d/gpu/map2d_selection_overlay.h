// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_GPU_MAP2D_SELECTION_OVERLAY_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_GPU_MAP2D_SELECTION_OVERLAY_H_

#include <cstdint>
#include <vector>

#include "vista/component/map/draw.h"

namespace content {

class GisScene;
class ViewFrame;

namespace detail {

// Stable signature over selected feature ids + flash pulse. Present uses this
// to break StaticReuse when only selection/flash changes (layout unchanged).
uint64_t map2d_selection_signature(const GisScene* scene, bool flash_pulse);

// Appends MapIR stroke meshes for selected features (and optional flash).
// Drawn by MapPass on the GPU present path — not HDC overlay_paint.
// Always strokes Feature::selected in orange; when |flash_pulse| also strokes
// selected_feature() in yellow (matches former HDC annotation + flash).
void append_map2d_selection_overlay(const GisScene* scene,
                                    const ViewFrame* frame,
                                    uint32_t width_px,
                                    uint32_t height_px,
                                    bool flash_pulse,
                                    vista::MapIR* ir);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_GPU_MAP2D_SELECTION_OVERLAY_H_
