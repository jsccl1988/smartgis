// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/layout/clip.h"

#include <memory>
#include <utility>
#include <vector>

#include "ogrsf_frmts.h"

namespace vista {
namespace detail {
namespace {

std::unique_ptr<OGRPolygon> make_aabb_polygon(const LayoutTile& tile) {
  auto ring = std::make_unique<OGRLinearRing>();
  ring->addPoint(tile.min_x, tile.min_y);
  ring->addPoint(tile.max_x, tile.min_y);
  ring->addPoint(tile.max_x, tile.max_y);
  ring->addPoint(tile.min_x, tile.max_y);
  ring->addPoint(tile.min_x, tile.min_y);
  auto poly = std::make_unique<OGRPolygon>();
  poly->addRingDirectly(ring.release());
  return poly;
}

}  // namespace

bool prepare_tile_clip(const OGRGeometry* geom, const LayoutTile* tile,
                       std::vector<std::unique_ptr<OGRGeometry>>* store,
                       const OGRGeometry** use, bool intersect) {
  if (!use) {
    return false;
  }
  *use = geom;
  if (!geom) {
    return false;
  }
  if (!tile) {
    return true;
  }
  OGREnvelope env;
  const_cast<OGRGeometry*>(geom)->getEnvelope(&env);
  if (!aabb_intersects(env.MinX, env.MinY, env.MaxX, env.MaxY, *tile)) {
    return false;
  }
  if (!intersect || !store ||
      aabb_contained(env.MinX, env.MinY, env.MaxX, env.MaxY, *tile)) {
    return true;
  }
  std::unique_ptr<OGRPolygon> clip = make_aabb_polygon(*tile);
  OGRGeometry* hit = const_cast<OGRGeometry*>(geom)->Intersection(clip.get());
  if (!hit || hit->IsEmpty()) {
    delete hit;
    return false;
  }
  store->emplace_back(hit);
  *use = store->back().get();
  return true;
}

bool prepare_tile_clip(const OGRGeometry* geom, const LayoutTile* tile,
                       std::vector<std::unique_ptr<OGRGeometry>>* store,
                       const OGRGeometry** use) {
  return prepare_tile_clip(geom, tile, store, use, /*intersect=*/true);
}

}  // namespace detail
}  // namespace vista
