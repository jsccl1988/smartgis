// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/tool/nav/view_zoom_apply.h"

#include <algorithm>
#include <cmath>

#include "gis/kernel/geo/mesh/geometry.h"
#include "gis/model/layer/layer.h"
#include "legacy/gis/present/carto/style_api.h"
#include "legacy/tool/defs.h"
#include "tool/draft/draft.h"

namespace tool {
namespace {

// Absolute-pan baseline: each MouseMove reapplies origin→end on wp0 captured
// at drag start. Wheel / ZoomToRect mutate the live windowport; if wp0 stays
// stale, the next click that reuses the same origin restores pre-zoom extent.
struct PanBaseline {
  render::LPRENDERDEVICE device = nullptr;
  base::Windowport wp0{};
  base::lPoint origin{};
};

PanBaseline& pan_baseline() {
  static PanBaseline baseline;
  return baseline;
}

void invalidate_pan_baseline() {
  pan_baseline().device = nullptr;
}

// Urgent async re-tessellate + sync HWND present (wheel / browse end).
void settle_browse_present(render::LPRENDERDEVICE device, SmtMap* map) {
  if (!device) {
    return;
  }
  if (map) {
    (void)device->ScheduleUrgentRedraw(map);
  }
  (void)device->RenderMap();
  (void)device->Refresh();
}

}  // namespace

void apply_wheel_zoom(render::LPRENDERDEVICE device, SmtMap* map, double scale_delt,
                      int z_delta, base::lPoint point) {
  if (!device) {
    return;
  }
  // SysPra.fZoomScaleDelt can be 0 / unset → fScale==1 and zoom_gate stays ~0.
  double delt = scale_delt;
  if (!(delt > 1e-6) || !std::isfinite(delt)) {
    delt = 0.12;
  }
  if (delt < 0.12) {
    delt = 0.12;
  }
  if (delt > 0.25) {
    delt = 0.25;
  }
  float fScale;
  if (z_delta < 0) {
    fScale = static_cast<float>(1 + delt);
  } else {
    fScale = static_cast<float>(1 - delt);
  }
  // MapLibre-like: stretch the last published front immediately; coalesce the
  // worker re-tessellate. Urgent settle every wheel notch fights the preview
  // and feels hitchy on china.
  device->PreviewZoomScale(point, fScale);
  (void)device->Refresh();
  device->ScheduleDelayedRedraw(map);
  invalidate_pan_baseline();
}

void apply_pan_by_points(render::LPRENDERDEVICE device, SmtMap* map,
                         base::lPoint origin, base::lPoint end,
                         bool gesture_end) {
  if (!device) {
    return;
  }
  // No-op click (origin==end): do not Refresh / ScheduleDelayedRedraw — that
  // re-encoded china on every map click and looked like a broken click.
  if (origin.x == end.x && origin.y == end.y) {
    return;
  }
  // Capture wp0 at drag start. During the stroke only pixel-slide the last
  // published front (SetCurDrawingOrg) — do not mutate the live windowport or
  // ScheduleDelayedRedraw. Mid-drag worker publish used to call
  // finish_interactive_settle (org=0 + new front at live wp), then the next
  // move restored wp0 and reapplied the full origin→end org → visual jump.
  PanBaseline& baseline = pan_baseline();
  if (baseline.device != device || baseline.origin.x != origin.x ||
      baseline.origin.y != origin.y) {
    baseline.device = device;
    baseline.wp0 = device->GetWindowport();
    baseline.origin = origin;
  }

  device->SetCurDrawingOrg(lPoint(end.x - origin.x, end.y - origin.y));
  device->Refresh();
  if (!gesture_end) {
    return;
  }

  device->SetWindowport(baseline.wp0);
  float x1 = 0.f, y1 = 0.f, x2 = 0.f, y2 = 0.f;
  device->DPToLP(origin.x, origin.y, x1, y1);
  device->DPToLP(end.x, end.y, x2, y2);
  device->PreviewZoomMove(fPoint(x2 - x1, y2 - y1));
  // Keep the pixel-slide org until a FrameJob newer than this commit publishes.
  // Clearing on a stale pre-commit settle snapped back then jumped again.
  device->set_clear_drawing_org_on_publish(
      true, device->map_published_generation());
  if (map) {
    (void)device->ScheduleUrgentRedraw(map);
  }
  (void)device->Refresh();
  invalidate_pan_baseline();
}

void apply_zoom_in_by_points(render::LPRENDERDEVICE device, SmtMap* map,
                             double scale_delt, base::lPoint origin,
                             base::lPoint end) {
  if (!device) {
    return;
  }
  if (origin != end) {
    fRect frt;
    lRect lrt;
    lrt.lb.x = (std::min)(origin.x, end.x);
    lrt.lb.y = (std::max)(origin.y, end.y);
    lrt.rt.x = (std::max)(origin.x, end.x);
    lrt.rt.y = (std::min)(origin.y, end.y);

    device->DRectToLRect(lrt, frt);
    device->ZoomToRect(map, frt);
  } else {
    device->ZoomScale(map, end, static_cast<float>(1 - scale_delt));
  }
  settle_browse_present(device, map);
  invalidate_pan_baseline();
}

void apply_zoom_out_at_point(render::LPRENDERDEVICE device, SmtMap* map,
                             double scale_delt, base::lPoint point) {
  if (!device) {
    return;
  }
  device->ZoomScale(map, point, static_cast<float>(1 + scale_delt));
  settle_browse_present(device, map);
  invalidate_pan_baseline();
}

void apply_zoom_restore(render::LPRENDERDEVICE device, SmtMap* map) {
  if (map == nullptr || device == nullptr) {
    return;
  }

  Envelope envelope;
  fRect frt;

  map->CalEnvelope();
  map->get_envelope(envelope);
  if (!envelope.is_init()) {
    SmtLayer* pLayer = map->GetActiveLayer();
    if (!pLayer) {
      return;
    }
    pLayer->CalEnvelope();
    pLayer->get_envelope(envelope);
    if (!envelope.is_init()) {
      return;
    }
  }

  envelope_to_rect(frt, envelope);

  float fWidthDiv = frt.width() / 40;
  float fHeightDiv = frt.height() / 40;
  if (fWidthDiv < 1e-6f) {
    fWidthDiv = 0.01f;
  }
  if (fHeightDiv < 1e-6f) {
    fHeightDiv = 0.01f;
  }

  frt.rt.x += fWidthDiv;
  frt.rt.y += fHeightDiv;
  frt.lb.x -= fWidthDiv;
  frt.lb.y -= fHeightDiv;

  device->ZoomToRect(map, frt, false);
  settle_browse_present(device, map);
  invalidate_pan_baseline();
}

void apply_zoom_refresh(render::LPRENDERDEVICE device, SmtMap* map) {
  if (map == nullptr || device == nullptr) {
    return;
  }
  SmtLayer* pLayer = map->GetActiveLayer();
  if (!pLayer) {
    return;
  }
  Envelope envelope;
  fRect frt;
  pLayer->CalEnvelope();
  pLayer->get_envelope(envelope);
  envelope_to_rect(frt, envelope);
  device->Refresh(map, frt);
  invalidate_pan_baseline();
}

void apply_view_draft(render::LPRENDERDEVICE device, SmtMap* map, double scale_delt,
                      int view_mode, base::lPoint* origin_inout,
                      BOOL* captured_inout, const Draft& draft) {
  if (draft.kind == DraftKind::kWheel) {
    lPoint pt{};
    if (!draft.points.empty()) {
      pt.x = draft.points.back().x_px;
      pt.y = draft.points.back().y_px;
    }
    apply_wheel_zoom(device, map, scale_delt, draft.wheel, pt);
    return;
  }
  if (!device || draft.points.empty() || !origin_inout || !captured_inout) {
    return;
  }
  lPoint end(draft.points.back().x_px, draft.points.back().y_px);
  if (draft.points.size() >= 2) {
    origin_inout->x = draft.points[0].x_px;
    origin_inout->y = draft.points[0].y_px;
  }
  *captured_inout = TRUE;
  const bool gesture_end = draft_flags::is_gesture_end(draft.flags);
  switch (view_mode) {
    case VM_ZoomIn:
      apply_zoom_in_by_points(device, map, scale_delt, *origin_inout, end);
      *captured_inout = FALSE;
      break;
    case VM_ZoomOut:
      apply_zoom_out_at_point(device, map, scale_delt, end);
      break;
    case VM_ZoomMove:
      apply_pan_by_points(device, map, *origin_inout, end, gesture_end);
      // Keep capture for the whole stroke; clearing every move broke origin.
      if (gesture_end) {
        *captured_inout = FALSE;
      }
      break;
    default:
      apply_pan_by_points(device, map, *origin_inout, end, gesture_end);
      if (gesture_end) {
        *captured_inout = FALSE;
      }
      break;
  }
}

}  // namespace tool
