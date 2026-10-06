// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_TERRAIN_MESH_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_TERRAIN_MESH_H_

#include <vector>

#include "content/browser/present/scene3d/frame/orbit_geo_frame.h"
#include "content/public/map_layer_types.h"
#include "vista/component/world/world.h"

namespace content {

class MapScene;

// Rebuild China DEM mesh into |world| and orbit-normalized |xyz|/|idx|.
// Skips work when LOD + extent still match |*lod_edge| / |*geo|.
// Caller owns present mutex when used from GPU/software paint paths.
void rebuild_terrain_mesh(vista::World* world,
                          const MapScene* scene,
                          const Extent2& extent,
                          float orbit_distance,
                          std::vector<float>* xyz,
                          std::vector<unsigned>* idx,
                          OrbitGeoFrame* geo,
                          int* lod_edge);

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_TERRAIN_MESH_H_
