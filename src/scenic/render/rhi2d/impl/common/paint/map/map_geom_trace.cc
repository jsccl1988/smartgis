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

}  // namespace detail
}  // namespace scenic
