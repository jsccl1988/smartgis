// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_SCENE3D_DEM_TO_WORLD_H_
#define SMT_LEGACY_RENDER_SCENE3D_DEM_TO_WORLD_H_

#include "gis/world/scene.h"
#include "legacy/render/scene3d/dem_height_field.h"

namespace render {

// Thin SP4 adapter: push DemHeightField lon/lat + exaggerated elev into
// gis::World as kTerrain, then attach a coarse CPU mesh (build_mesh) so
// GpuScene::sync_from / rebuild_meshes can upload triangles (not only AABB).
// World Z carries elev (GIS AABB); mesh XYZ stays leftover Y-up
// (lon, elev, lat). |max_edge| caps DEM downsample (default 96).
SCENE3D_EXPORT_API gis::Node* seed_dem_height_field_into_world(
    gis::World* world, const DemHeightField& dem, const char* name,
    int max_edge = 96);

}  // namespace render

#endif  // SMT_LEGACY_RENDER_SCENE3D_DEM_TO_WORLD_H_
