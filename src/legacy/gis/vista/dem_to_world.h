// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_GIS_VISTA_DEM_TO_WORLD_H_
#define SMT_LEGACY_GIS_VISTA_DEM_TO_WORLD_H_

#include "gis/gis_export.h"
#include "gis/vista/world/world.h"
#include "legacy/gis/vista/dem_height_field.h"

namespace render {

// Thin SP4 adapter: forward DemHeightField's DemRaster into
// gis::seed_dem_raster_into_world (kTerrain + CPU mesh + optional UV/tex).
// World Z carries elev (GIS AABB); mesh XYZ stays leftover Y-up
// (X=-lon, elev, lat). |max_edge| caps DEM downsample (default 96).
GIS_EXPORT gis::Node* seed_dem_height_field_into_world(
    gis::World* world, const DemHeightField& dem, const char* name,
    int max_edge = 96);

}  // namespace render

#endif  // SMT_LEGACY_GIS_VISTA_DEM_TO_WORLD_H_
