// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_TOOL_GROUP_VIEW_ZOOM_APPLY_H_
#define LEGACY_TOOL_GROUP_VIEW_ZOOM_APPLY_H_

#include "gis/map/map.h"
#include "legacy/core/macros/macros.h"
#include "legacy/core/types/types.h"
#include "legacy/render/rhi2d/public/device/renderdevice.h"
#include "tool/draft/draft.h"

namespace tool {

// 2D leftover view side-effects (device + map). No HWND / Workspace.

void apply_wheel_zoom(render::LPRENDERDEVICE device, Map* map,
                      double scale_delt, int z_delta, base::lPoint point);

void apply_pan_by_points(render::LPRENDERDEVICE device, Map* map,
                         base::lPoint origin, base::lPoint end,
                         bool gesture_end = false);

void apply_zoom_in_by_points(render::LPRENDERDEVICE device, Map* map,
                             double scale_delt, base::lPoint origin,
                             base::lPoint end);

void apply_zoom_out_at_point(render::LPRENDERDEVICE device, Map* map,
                             double scale_delt, base::lPoint point);

void apply_zoom_restore(render::LPRENDERDEVICE device, Map* map);

void apply_zoom_refresh(render::LPRENDERDEVICE device, Map* map);

// Dispatches a completed navigate Draft for leftover ViewCtrl modes.
// |view_mode| is eViewMode (VM_ZoomIn / Out / Move / …).
void apply_view_draft(render::LPRENDERDEVICE device, Map* map,
                      double scale_delt, int view_mode,
                      base::lPoint* origin_inout, BOOL* captured_inout,
                      const Draft& draft);

}  // namespace tool

#endif  // LEGACY_TOOL_GROUP_VIEW_ZOOM_APPLY_H_
