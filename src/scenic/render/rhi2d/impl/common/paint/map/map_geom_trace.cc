// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/paint/map/map_geom_trace.h"

#include <chrono>

#include "base/trace/event/process_trace.h"
#include "gis/map/map.h"
#include "ogrsf_frmts.h"

namespace scenic {
namespace detail {

thread_local GeomUsAccum* g_active_geom = nullptr;

void flush_geom_us(const GeomUsAccum& a) {
  if (!base::trace::tracing_enabled()) {
    return;
  }
  const auto now = base::trace::Trace::time_point::clock::now();
  auto emit = [&](const char* name, int64_t us) {
    if (us <= 0) {
      return;
    }
    base::trace::process_trace_add(name, "gdi.geom",
                                   now - std::chrono::microseconds(us), now);
  };
  emit("point", a.point);
  emit("line", a.line);
  emit("polygon", a.polygon);
  emit("multipoint", a.multipoint);
  emit("multiline", a.multiline);
  emit("multipolygon", a.multipolygon);
  emit("ring", a.ring);
  emit("anno", a.anno);
  emit("image", a.image);
}

std::string trace_name(OGRLayer* layer) {
  if (!layer) {
    return "ogr";
  }
  const char* n = layer->GetName();
  return (n && n[0]) ? std::string(n) : std::string("ogr");
}

void add_geom_draw_us(int type, bool is_anno, int64_t us) {
  if (!g_active_geom || us <= 0) {
    return;
  }
  if (is_anno) {
    g_active_geom->anno += us;
    return;
  }
  switch (wkbFlatten(static_cast<OGRwkbGeometryType>(type))) {
    case wkbPoint:
      g_active_geom->point += us;
      break;
    case wkbLineString:
      g_active_geom->line += us;
      break;
    case wkbPolygon:
    case wkbTriangle:
      g_active_geom->polygon += us;
      break;
    case wkbMultiPoint:
      g_active_geom->multipoint += us;
      break;
    case wkbMultiLineString:
      g_active_geom->multiline += us;
      break;
    case wkbMultiPolygon:
    case wkbTIN:
      g_active_geom->multipolygon += us;
      break;
    case wkbLinearRing:
      g_active_geom->ring += us;
      break;
    default:
      break;
  }
}

}  // namespace detail
}  // namespace scenic
