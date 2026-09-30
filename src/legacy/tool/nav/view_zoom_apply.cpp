// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/tool/nav/view_zoom_apply.h"

#include <algorithm>

#include "gis/kernel/geo/mesh/geometry.h"
#include "gis/model/layer/layer.h"
#include "legacy/gis/present/carto/style_api.h"
#include "legacy/tool/defs.h"
#include "tool/draft/draft.h"

namespace tool {

void apply_wheel_zoom(render::LPRENDERDEVICE device, SmtMap* map, double scale_delt,
                      int z_delta, base::lPoint point) {
  if (!device) {
    return;
  }
  float fScale;
  if (z_delta < 0) {
    fScale = static_cast<float>(1 + scale_delt);
  } else {
    fScale = static_cast<float>(1 - scale_delt);
  }
  device->PreviewZoomScale(point, fScale);
  device->Refresh();
  device->ScheduleDelayedRedraw(map);
}

void apply_pan_by_points(render::LPRENDERDEVICE device, SmtMap* map,
                         base::lPoint origin, base::lPoint end) {
  if (!device) {
    return;
  }
  // Absolute pan from drag start: each MouseMove used to PreviewZoomMove the
  // full origin→end delta on an already-mutated windowport (runaway pan) and
  // zeroed curDrawingOrg so Refresh showed a stale unshifted front until the
  // worker finished (click / settle to see the map move).
  struct PanBaseline {
    render::LPRENDERDEVICE device = nullptr;
    base::Windowport wp0{};
    base::lPoint origin{};
  };
  static PanBaseline baseline;
  if (baseline.device != device || baseline.origin.x != origin.x ||
      baseline.origin.y != origin.y) {
    baseline.device = device;
    baseline.wp0 = device->GetWindowport();
    baseline.origin = origin;
  }

  device->SetWindowport(baseline.wp0);
  float x1 = 0.f, y1 = 0.f, x2 = 0.f, y2 = 0.f;
  device->DPToLP(origin.x, origin.y, x1, y1);
  device->DPToLP(end.x, end.y, x2, y2);
  device->PreviewZoomMove(fPoint(x2 - x1, y2 - y1));

  // Slide the last published front with the pointer while the worker catches up.
  device->SetCurDrawingOrg(lPoint(end.x - origin.x, end.y - origin.y));
  device->Refresh();
  device->ScheduleDelayedRedraw(map);
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
  device->Refresh();
}

void apply_zoom_out_at_point(render::LPRENDERDEVICE device, SmtMap* map,
                             double scale_delt, base::lPoint point) {
  if (!device) {
    return;
  }
  device->ZoomScale(map, point, static_cast<float>(1 + scale_delt));
  device->Refresh();
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

  // Proxy re-render only — avoid Refresh() race with worker GDI buffers.
  device->ZoomToRect(map, frt, false);
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
  switch (view_mode) {
    case VM_ZoomIn:
      apply_zoom_in_by_points(device, map, scale_delt, *origin_inout, end);
      *captured_inout = FALSE;
      break;
    case VM_ZoomOut:
      apply_zoom_out_at_point(device, map, scale_delt, end);
      break;
    case VM_ZoomMove:
      apply_pan_by_points(device, map, *origin_inout, end);
      *captured_inout = FALSE;
      break;
    default:
      apply_pan_by_points(device, map, *origin_inout, end);
      *captured_inout = FALSE;
      break;
  }
}

}  // namespace tool
