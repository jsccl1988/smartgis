// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_GIS_VISTA_DEM_TO_WORLD_H_
#define SMT_LEGACY_GIS_VISTA_DEM_TO_WORLD_H_

#include "vista/vista_export.h"
#include "vista/world/world.h"
#include "vista/world/terrain/dem/dem_height_field.h"

namespace render {

// Thin SP4 adapter: forward DemHeightField's DemRaster into
// vista::seed_dem_raster_into_world (kTerrain + CPU mesh + optional UV/tex).
// World Z carries elev (GIS AABB); mesh XYZ stays leftover Y-up
// (X=-lon, elev, lat). |max_edge| caps DEM downsample (default 512 for
// china_dem 1536×960 national framing).
VISTA_EXPORT vista::Node* seed_dem_height_field_into_world(
    vista::World* world, const DemHeightField& dem, const char* name,
    int max_edge = 512);

}  // namespace render

#endif  // SMT_LEGACY_GIS_VISTA_DEM_TO_WORLD_H_
