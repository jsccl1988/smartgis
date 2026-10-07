// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/layout/pack.h"

#include "gis/envelope.h"
#include "gis/geo/ops/geometry_traits.h"
#include "ogrsf_frmts.h"
#include "vista/component/map/layout/slice_key.h"
#include "vista/component/map/layout/view_metrics.h"

namespace vista {
namespace detail {
namespace {

gis::Envelope view_pack_envelope(const View& view) {
  gis::Envelope env;
  const double outset = layout_tile_outset_world(view);
  env.MinX = view.min_x - outset;
  env.MinY = view.min_y - outset;
  env.MaxX = view.max_x + outset;
  env.MaxY = view.max_y + outset;
  return env;
}

bool env_too_small_on_device(const gis::Envelope& env, float scale) {
  const float dx = static_cast<float>((env.MaxX - env.MinX) * scale);
  const float dy = static_cast<float>((env.MaxY - env.MinY) * scale);
  const float min_px = scale < 12.f ? 3.f : 2.f;
  return dx < min_px && dy < min_px;
}

std::unique_ptr<OGRGeometry> clone_without_holes(const OGRGeometry* geom) {
  if (!geom || geom->IsEmpty()) {
    return nullptr;
  }
  OGRGeometry* raw = const_cast<OGRGeometry*>(geom);
  const OGRwkbGeometryType type = wkbFlatten(raw->getGeometryType());
  if (type == wkbPolygon) {
    auto* poly = dynamic_cast<OGRPolygon*>(raw);
    if (!poly || poly->getExteriorRing() == nullptr) {
      return nullptr;
    }
    if (poly->getNumInteriorRings() == 0) {
      return nullptr;
    }
    auto out = std::make_unique<OGRPolygon>();
    out->addRing(poly->getExteriorRing());
    return out;
  }
  if (type == wkbMultiPolygon) {
    auto* mp = dynamic_cast<OGRMultiPolygon*>(raw);
    if (!mp) {
      return nullptr;
    }
    bool any_holes = false;
    auto out = std::make_unique<OGRMultiPolygon>();
    for (int i = 0; i < mp->getNumGeometries(); ++i) {
      auto stripped = clone_without_holes(mp->getGeometryRef(i));
      if (stripped) {
        any_holes = true;
        out->addGeometryDirectly(stripped.release());
      } else if (OGRGeometry* part = mp->getGeometryRef(i)) {
        out->addGeometry(part);
      }
    }
    if (!any_holes) {
      return nullptr;
    }
    return out;
  }
  if (auto* coll = dynamic_cast<OGRGeometryCollection*>(raw)) {
    if (type == wkbMultiPolygon) {
      return nullptr;
    }
    bool any = false;
    auto out = std::make_unique<OGRGeometryCollection>();
    for (int i = 0; i < coll->getNumGeometries(); ++i) {
      auto stripped = clone_without_holes(coll->getGeometryRef(i));
      if (stripped) {
        any = true;
        out->addGeometryDirectly(stripped.release());
      } else if (OGRGeometry* part = coll->getGeometryRef(i)) {
        out->addGeometry(part);
      }
    }
    if (!any) {
      return nullptr;
    }
    return out;
  }
  return nullptr;
}

}  // namespace

PackedGeoms::PackedGeoms() = default;
PackedGeoms::~PackedGeoms() = default;
PackedGeoms::PackedGeoms(PackedGeoms&&) noexcept = default;
PackedGeoms& PackedGeoms::operator=(PackedGeoms&&) noexcept = default;

PackedGeoms pack_geoms(const LayoutInput& in,
                       const std::vector<LayerBatch>& layers) {
  PackedGeoms packed;
  if (!screen_ready(in.view)) {
    packed.batches = layers;
    return packed;
  }
  const gis::Envelope view_env = view_pack_envelope(in.view);
  const float scale = device_px_per_world(in.view);
  const bool drop_holes = scale < 12.f;
  packed.batches.reserve(layers.size());
  for (const LayerBatch& src : layers) {
    LayerBatch dst;
    dst.source_layer = src.source_layer;
    dst.geoms.reserve(src.geoms.size());
    dst.attrs.reserve(src.attrs.size());
    for (size_t i = 0; i < src.geoms.size(); ++i) {
      const OGRGeometry* geom = src.geoms[i];
      if (!geom || geom->IsEmpty()) {
        continue;
      }
      gis::Envelope env;
      geo::fill_envelope(*geom, &env);
      if (!env.intersects(view_env)) {
        continue;
      }
      const OGRwkbGeometryType type =
          wkbFlatten(const_cast<OGRGeometry*>(geom)->getGeometryType());
      if (type != wkbPoint && type != wkbMultiPoint &&
          env_too_small_on_device(env, scale)) {
        continue;
      }
      const OGRGeometry* use = geom;
      if (drop_holes) {
        auto stripped = clone_without_holes(geom);
        if (stripped) {
          use = stripped.get();
          packed.owned.push_back(std::move(stripped));
        }
      }
      dst.geoms.push_back(use);
      if (i < src.attrs.size()) {
        dst.attrs.push_back(src.attrs[i]);
      }
    }
    packed.batches.push_back(std::move(dst));
  }
  return packed;
}

}  // namespace detail
}  // namespace vista
