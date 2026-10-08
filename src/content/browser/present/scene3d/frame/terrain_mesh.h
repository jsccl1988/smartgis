// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_TERRAIN_MESH_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_TERRAIN_MESH_H_

#include <vector>

#include "content/browser/present/scene3d/frame/orbit_geo_frame.h"
#include "content/public/types.h"
#include "vista/component/world/world.h"

namespace content {

class GisScene;

// Thin adapter: forwards to vista::rebuild_dem_view_mesh (China-box, LOD skip,
// orbit normalize). Caller owns present mutex on GPU/hdc paint paths.
void rebuild_terrain_mesh(vista::World* world,
                          const GisScene* scene,
                          const Extent2& extent,
                          float orbit_distance,
                          std::vector<float>* xyz,
                          std::vector<unsigned>* idx,
                          OrbitGeoFrame* geo,
                          int* lod_edge);

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_TERRAIN_MESH_H_
