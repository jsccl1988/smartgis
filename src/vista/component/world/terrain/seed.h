// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Seed DEM / TIN / heightfield surface into World kTerrain TerrainPayload.
// Mesh build stays on DemRaster / mesh tess; this file only writes World.

#ifndef VISTA_COMPONENT_WORLD_TERRAIN_SEED_H_
#define VISTA_COMPONENT_WORLD_TERRAIN_SEED_H_

#include <cstddef>

#include "vista/component/world/terrain/lod.h"
#include "vista/terrain/dem/dem_height_field.h"
#include "vista/terrain/dem/dem_raster.h"
#include "vista/vista_export.h"
#include "vista/component/world/world.h"

class OGRTriangulatedSurface;

namespace vista {

// --- Raster DEM (grid downsample) ---

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
VISTA_EXPORT size_t seed_dem_view_tiles_into_world(
    World* world, const DemRaster& dem, double view_minx, double view_miny,
    double view_maxx, double view_maxy, float camera_distance,
    int max_total_vertices = 65536, const char* name_prefix = "dem_tile");

// Nested-grid / CPU-CDLOD: concentric rings around the view center. Each
// tile's max_edge comes from patch-camera distance (radial from view center)
// then DemRaster::build_mesh_window. CPU morph weights + Y-up edge skirts
// hide T-junctions. Not GPU tess or geometry clipmap.
VISTA_EXPORT size_t seed_dem_nested_grid_into_world(
    World* world, const DemRaster& dem, double view_minx, double view_miny,
    double view_maxx, double view_maxy, float camera_distance,
    int max_total_vertices = 65536, const char* name_prefix = "nested");

// Same nested seed, rings centered on |focus_x,focus_y| (lon/lat, clamped).
VISTA_EXPORT size_t seed_dem_nested_grid_into_world(
    World* world, const DemRaster& dem, double view_minx, double view_miny,
    double view_maxx, double view_maxy, float camera_distance,
    int max_total_vertices, const char* name_prefix, double focus_x,
    double focus_y);

// Views / shell helper: load sample DEM, optional land mask, seed World.
VISTA_EXPORT Node* seed_china_dem_into_world(
    World* world, const LonLatRing* rings, size_t ring_count, const char* name,
    int max_edge = 96);

// --- Heightfield surface-interp (DemHeightField::sample / build_mesh) ---

// Seed kTerrain from DemHeightField at |max_edge| (surface sample density).
VISTA_EXPORT Node* seed_dem_surface_into_world(World* world,
                                               const render::DemHeightField& dem,
                                               const char* name,
                                               int max_edge = 96);

// LOD variant: max_edge from terrain_lod_surface_edge(camera_distance).
VISTA_EXPORT Node* seed_dem_surface_lod_into_world(
    World* world, const render::DemHeightField& dem, const char* name,
    float camera_distance);

// --- TIN / OGRTriangulatedSurface ---

// Tessellate |tin| into a kTerrain TerrainPayload (full density, stride=1).
VISTA_EXPORT Node* seed_tin_into_world(World* world,
                                       const OGRTriangulatedSurface* tin,
                                       const char* name);

// Tessellate |tin| then thin triangles. Without camera XYZ this is discrete
// keep-every-Nth via terrain_lod_tin_stride(|camera_distance|).
VISTA_EXPORT Node* seed_tin_lod_into_world(World* world,
                                           const OGRTriangulatedSurface* tin,
                                           const char* name,
                                           float camera_distance);

// Spatial thin: triangle-centroid distance to |camera_x,y,z| (same space as
// tessellate_3d_surface positions). Near triangles stay denser than far in
// one seed. |camera_distance| is the near-bucket floor.
VISTA_EXPORT Node* seed_tin_lod_into_world(World* world,
                                           const OGRTriangulatedSurface* tin,
                                           const char* name,
                                           float camera_distance,
                                           float camera_x, float camera_y,
                                           float camera_z);

// Same spatial thin using a tileset ViewState already in world units.
VISTA_EXPORT Node* seed_tin_lod_into_world(World* world,
                                           const OGRTriangulatedSurface* tin,
                                           const char* name,
                                           float camera_distance,
                                           const ViewState& view);

}  // namespace vista

namespace render {

// Thin adapter: forward DemHeightField's DemRaster into
// vista::seed_dem_raster_into_world (kTerrain + CPU mesh + optional UV/tex).
VISTA_EXPORT vista::Node* seed_dem_height_field_into_world(
    vista::World* world, const render::DemHeightField& dem, const char* name,
    int max_edge = 512);

}  // namespace render

#endif  // VISTA_COMPONENT_WORLD_TERRAIN_SEED_H_
