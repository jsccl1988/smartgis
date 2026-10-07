// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/frame/terrain_mesh.h"

#include "content/browser/document/gis_scene.h"
#include "vista/component/world/terrain/mesh.h"

namespace content {

void rebuild_terrain_mesh(vista::World* world,
                          const GisScene* scene,
                          const Extent2& extent,
                          float orbit_distance,
                          std::vector<float>* xyz,
                          std::vector<unsigned>* idx,
                          OrbitGeoFrame* geo,
                          int* lod_edge) {
  if (!world || !xyz || !idx || !geo || !lod_edge) {
    return;
  }
  (void)scene;
  vista::OrbitGeoFrame vgeo = geo->as_vista();
  (void)vista::rebuild_dem_view_mesh(world, extent.xmin, extent.ymin,
                                     extent.xmax, extent.ymax, orbit_distance,
                                     xyz, idx, &vgeo, lod_edge);
  geo->sync_from(vgeo);
}

}  // namespace content
