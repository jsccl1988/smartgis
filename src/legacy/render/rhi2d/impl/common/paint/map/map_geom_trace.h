// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_RHI2D_MAP_GEOM_TRACE_H_
#define SMT_LEGACY_RENDER_RHI2D_MAP_GEOM_TRACE_H_

#include <cstdint>
#include <string>

#include "legacy/gis/layer/layer.h"

class OGRLayer;

namespace render {
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

std::string trace_name(OGRLayer* layer);
std::string trace_name(const gis::Layer* layer);

}  // namespace detail
}  // namespace render

#endif  // SMT_LEGACY_RENDER_RHI2D_MAP_GEOM_TRACE_H_
