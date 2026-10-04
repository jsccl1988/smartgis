// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_MAP_GEOM_TRACE_H_
#define SCENIC_RHI2D_MAP_GEOM_TRACE_H_

#include <cstdint>
#include <string>

class OGRLayer;

namespace scenic {
namespace detail {

// Per-layer GDI draw timing (us) rolled into process_trace on flush.
struct GeomUsAccum {
  int64_t point = 0;
  int64_t line = 0;
  int64_t polygon = 0;
  int64_t multipoint = 0;
  int64_t multiline = 0;
  int64_t multipolygon = 0;
  int64_t ring = 0;
  int64_t anno = 0;
  int64_t image = 0;
};

// Active accumulator for the current layer play / sync draw path.
extern thread_local GeomUsAccum* g_active_geom;

void flush_geom_us(const GeomUsAccum& a);

struct GeomTraceScope {
  GeomUsAccum accum;
  GeomTraceScope() { g_active_geom = &accum; }
  ~GeomTraceScope() {
    g_active_geom = nullptr;
    flush_geom_us(accum);
  }
  GeomTraceScope(const GeomTraceScope&) = delete;
  GeomTraceScope& operator=(const GeomTraceScope&) = delete;
};

void add_geom_draw_us(int type, bool is_anno, int64_t us);

std::string trace_name(OGRLayer* layer);

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_MAP_GEOM_TRACE_H_
