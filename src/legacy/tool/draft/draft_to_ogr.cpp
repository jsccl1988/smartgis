// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/tool/draft/draft_to_ogr.h"

#include <cstdint>

#include "legacy/tool/defs.h"
#include "ogrsf_frmts.h"

namespace tool {
namespace {

void add_draft_points(render::LPRENDERDEVICE device, const Draft& draft,
                      OGRLineString* geom) {
  for (const DraftPoint& p : draft.points) {
    float x = 0;
    float y = 0;
    device->DPToLP(p.x_px, p.y_px, x, y);
    geom->addPoint(x, y);
  }
}

void add_rect_ring(render::LPRENDERDEVICE device, const Draft& draft,
                   OGRLineString* geom) {
  if (draft.points.size() < 2) {
    add_draft_points(device, draft, geom);
    return;
  }
  const int32_t x0 = draft.points[0].x_px;
  const int32_t y0 = draft.points[0].y_px;
  const int32_t x1 = draft.points[1].x_px;
  const int32_t y1 = draft.points[1].y_px;
  const DraftPoint corners[] = {
      {x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}, {x0, y0},
  };
  for (const DraftPoint& p : corners) {
    float x = 0;
    float y = 0;
    device->DPToLP(p.x_px, p.y_px, x, y);
    geom->addPoint(x, y);
  }
}

}  // namespace

OGRPoint* ogr_point_from_draft(render::LPRENDERDEVICE device,
                               const Draft& draft) {
  if (!device || draft.points.empty()) {
    return nullptr;
  }
  float x = 0;
  float y = 0;
  device->DPToLP(draft.points[0].x_px, draft.points[0].y_px, x, y);
  return new OGRPoint(x, y);
}

OGRGeometry* ogr_line_from_draft(render::LPRENDERDEVICE device,
                                 const Draft& draft, ushort line_type) {
  if (!device || draft.points.empty()) {
    return nullptr;
  }
  if (line_type == LT_LinearRing || draft.kind == DraftKind::kPolygon) {
    OGRLinearRing* ring = new OGRLinearRing();
    if (draft.kind == DraftKind::kRect) {
      add_rect_ring(device, draft, ring);
    } else {
      add_draft_points(device, draft, ring);
    }
    ring->closeRings();
    return ring;
  }
  if (line_type == LT_Spline_Lag || line_type == LT_Spline_Bzer ||
      line_type == LT_Spline_B || line_type == LT_Spline_3) {
    OGRLineString* spline = new OGRLineString();
    add_draft_points(device, draft, spline);
    return spline;
  }
  OGRLineString* line = new OGRLineString();
  if (draft.kind == DraftKind::kRect || line_type == LT_Rect) {
    add_rect_ring(device, draft, line);
  } else {
    add_draft_points(device, draft, line);
  }
  return line;
}

OGRGeometry* ogr_region_from_draft(render::LPRENDERDEVICE device,
                                   const Draft& draft) {
  if (!device || draft.points.empty()) {
    return nullptr;
  }
  OGRLinearRing* ring = new OGRLinearRing();
  if (draft.kind == DraftKind::kRect) {
    add_rect_ring(device, draft, ring);
  } else {
    add_draft_points(device, draft, ring);
  }
  ring->closeRings();
  OGRPolygon* poly = new OGRPolygon();
  poly->addRingDirectly(ring);
  return poly;
}

}  // namespace tool
