// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/map/layout/geom_walk.h"

namespace vista {
namespace detail {

bool layer_uses_batch(const gis::style::StyleLayer& layer,
                      const LayerBatch& batch) {
  return layer.source_layer.empty() || layer.source_layer == batch.source_layer;
}

bool next_codepoint(const std::string& text, size_t* index, uint32_t* cp) {
  if (*index >= text.size()) {
    return false;
  }
  const auto* p =
      reinterpret_cast<const unsigned char*>(text.data() + *index);
  const size_t left = text.size() - *index;
  if (p[0] < 0x80) {
    *cp = p[0];
    *index += 1;
    return true;
  }
  if ((p[0] & 0xe0) == 0xc0 && left >= 2) {
    *cp = (static_cast<uint32_t>(p[0] & 0x1f) << 6) |
          static_cast<uint32_t>(p[1] & 0x3f);
    *index += 2;
    return true;
  }
  if ((p[0] & 0xf0) == 0xe0 && left >= 3) {
    *cp = (static_cast<uint32_t>(p[0] & 0x0f) << 12) |
          (static_cast<uint32_t>(p[1] & 0x3f) << 6) |
          static_cast<uint32_t>(p[2] & 0x3f);
    *index += 3;
    return true;
  }
  if ((p[0] & 0xf8) == 0xf0 && left >= 4) {
    *cp = (static_cast<uint32_t>(p[0] & 0x07) << 18) |
          (static_cast<uint32_t>(p[1] & 0x3f) << 12) |
          (static_cast<uint32_t>(p[2] & 0x3f) << 6) |
          static_cast<uint32_t>(p[3] & 0x3f);
    *index += 4;
    return true;
  }
  *cp = p[0];
  *index += 1;
  return true;
}

const OGRLineString* first_line(const OGRGeometry* geom) {
  const OGRLineString* found = nullptr;
  for_each_line(geom, [&](const OGRLineString* line) {
    if (!found && line && line->getNumPoints() >= 2) {
      found = line;
    }
  });
  return found;
}

bool anchor_xy(const OGRGeometry* geom, double* x, double* y) {
  if (!geom || geom->IsEmpty() || !x || !y) {
    return false;
  }
  if (const auto* pt = dynamic_cast<const OGRPoint*>(geom)) {
    *x = pt->getX();
    *y = pt->getY();
    return true;
  }
  if (const auto* multi = dynamic_cast<const OGRMultiPoint*>(geom)) {
    if (multi->getNumGeometries() > 0) {
      return anchor_xy(multi->getGeometryRef(0), x, y);
    }
  }
  OGREnvelope env;
  geom->getEnvelope(&env);
  *x = (env.MinX + env.MaxX) * 0.5;
  *y = (env.MinY + env.MaxY) * 0.5;
  return true;
}

}  // namespace detail
}  // namespace vista
