// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Attach a DemRaster (or DemHeightField shell) onto a World kTerrain node.
// Mesh build stays on DemRaster; this file only writes World.

#ifndef VISTA_COMPONENT_WORLD_DEM_SEED_H_
#define VISTA_COMPONENT_WORLD_DEM_SEED_H_

#include <cstddef>

#include "vista/terrain/dem/dem_height_field.h"
#include "vista/terrain/dem/dem_raster.h"
#include "vista/vista_export.h"
#include "vista/component/world/world.h"

namespace vista {

// Attach kTerrain + coarse mesh from |dem| into |world|.
VISTA_EXPORT Node* seed_dem_raster_into_world(World* world, const DemRaster& dem,
                                            const char* name,
                                            int max_edge = 96);

// Same as seed_dem_raster_into_world but picks max_edge from camera distance.
VISTA_EXPORT Node* seed_dem_raster_lod_into_world(World* world,
                                                const DemRaster& dem,
                                                const char* name,
                                                float camera_distance);

// Seed one or more kTerrain tiles covering the lon/lat view AABB. Closer
// camera → more tiles / denser max_edge. Total vertices stay under
// |max_total_vertices|. Returns the number of terrain nodes attached.
// Documented interactive budget: 65536 verts (china overview stays well under).
VISTA_EXPORT size_t seed_dem_view_tiles_into_world(
    World* world, const DemRaster& dem, double view_minx, double view_miny,
    double view_maxx, double view_maxy, float camera_distance,
    int max_total_vertices = 65536, const char* name_prefix = "dem_tile");

// Views / shell helper: load sample DEM, optional land mask, seed World.
// Does not touch leftover DemHeightField.
VISTA_EXPORT Node* seed_china_dem_into_world(
    World* world, const LonLatRing* rings, size_t ring_count, const char* name,
    int max_edge = 96);

}  // namespace vista

namespace render {

// Thin adapter: forward DemHeightField's DemRaster into
// vista::seed_dem_raster_into_world (kTerrain + CPU mesh + optional UV/tex).
// World Z carries elev (GIS AABB); mesh XYZ stays leftover Y-up
// (X=-lon, elev, lat). |max_edge| caps DEM downsample (default 512 for
// china_dem 1536×960 national framing).
VISTA_EXPORT vista::Node* seed_dem_height_field_into_world(
    vista::World* world, const DemHeightField& dem, const char* name,
    int max_edge = 512);

}  // namespace render

#endif  // VISTA_COMPONENT_WORLD_DEM_SEED_H_
