// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/frame/terrain_mesh.h"

#include <vector>

#include "content/browser/document/map_scene.h"
#include "vista/component/world/terrain/mesh.h"
#include "vista/terrain/dem/raster/dem_raster.h"

namespace content {
namespace {

float capture_orbit_elev(const std::vector<float>& positions, void* ctx) {
  auto* geo = static_cast<OrbitGeoFrame*>(ctx);
  geo->capture_elev_center(positions);
  return geo->cy;
}

void normalize_orbit_xyz(std::vector<float>* positions, void* ctx) {
  static_cast<OrbitGeoFrame*>(ctx)->normalize_xyz(positions);
}

}  // namespace

void rebuild_terrain_mesh(vista::World* world,
                          const MapScene* scene,
                          const Extent2& extent,
                          float orbit_distance,
                          std::vector<float>* xyz,
                          std::vector<unsigned>* idx,
                          OrbitGeoFrame* geo,
                          int* lod_edge) {
  if (!world || !xyz || !idx || !geo || !lod_edge) {
    return;
  }
  // China-box fallback and the LOD skip stay next to the orbit frame.
  // A set sample_dem_path_override must not force the China box.
  double box_minx = extent.xmin;
  double box_miny = extent.ymin;
  double box_maxx = extent.xmax;
  double box_maxy = extent.ymax;
  vista::dem_seed_lonlat_box(extent.xmin, extent.ymin, extent.xmax, extent.ymax,
                           &box_minx, &box_miny, &box_maxx, &box_maxy);
  Extent2 frame;
  frame.xmin = box_minx;
  frame.ymin = box_miny;
  frame.xmax = box_maxx;
  frame.ymax = box_maxy;
  const int cache_key = vista::dem_seed_cache_key(orbit_distance);
  if (!xyz->empty() && !idx->empty() && *lod_edge == cache_key &&
      geo->matches_extent(frame)) {
    return;
  }

  *geo = OrbitGeoFrame::from_extent(frame);
  (void)scene;
  vista::DemViewMeshHooks hooks;
  hooks.capture_elev_center = &capture_orbit_elev;
  hooks.normalize_xyz = &normalize_orbit_xyz;
  hooks.ctx = geo;
  const vista::DemViewMeshResult result = vista::rebuild_dem_view_mesh(
      world, frame.xmin, frame.ymin, frame.xmax, frame.ymax, orbit_distance,
      xyz, idx, &hooks);
  if (result.seed_cache_hit) {
    geo->cy = result.elev_cy;
  }
  if (result.painted) {
    *lod_edge = result.cache_key;
  }
}

}  // namespace content
