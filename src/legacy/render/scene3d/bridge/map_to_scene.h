// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_SCENE3D_MAP_TO_SCENE_H_
#define SMT_LEGACY_RENDER_SCENE3D_MAP_TO_SCENE_H_

#include "gis/vista/world/world.h"
#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/camera/camera.h"
#include "legacy/render/rhi3d/public/device/render_device.h"
#include "legacy/render/scene3d/dem/dem_height_field.h"
#include "legacy/render/scene3d/scene/scene.h"
#include "legacy/render/scene3d/scene/vertex3d.h"

class OGRLayer;

namespace render {

// 400x300-class leftover 3D viewport: sane perspective (never zNear=0).
LEGACY_RENDER_EXPORT void apply_view3d_viewport(Viewport3D* vp, ulong width,
                                                ulong height);

// Leftover Y-up pose that frames |aabb| (X=-lon, Y=elev, Z=lat).
// Empty AABB falls back to origin + span 40 (legacy cube).
LEGACY_RENDER_EXPORT void leftover_frame_pose(const Aabb& aabb, Vector3* eye,
                                              Vector3* target, float* span);

// Scene DEM leftover AABB (X=-lon, height→Y, lat→Z). False when no DEM seeded.
// Uses the last successful seed's framing cache (does not block another scene).
LEGACY_RENDER_EXPORT bool leftover_dem_aabb(Aabb* out);

// True when |eye| is still the leftover origin pose (not over the DEM).
LEGACY_RENDER_EXPORT bool leftover_eye_misses_dem(const Vector3& eye);

// True after any successful stereo DEM seed (last frame cache). Not a sticky
// "already have DEM → skip seeding this SmtScene" gate.
LEGACY_RENDER_EXPORT bool leftover_has_scene_dem();

// Place a perspective camera so the DEM (preferred) or scene AABB fills the
// frustum. Always raises zFar from the framed span.
LEGACY_RENDER_EXPORT void frame_persp_camera_to_aabb(SmtPerspCamera* camera,
                                                     Viewport3D* vp,
                                                     const Aabb& aabb);

// Lift an OGR layer (china_plp regions/lines/points/labels) into leftover 3D
// objects. Polygons/lines drape on DemHeightField; place-names go to labels.
// Returns the number of objects added. Safe when style is missing on features.
// SP4: stereo DEM also seeds map_seeded_world() via
// seed_dem_height_field_into_world (envelope + CPU mesh for GpuScene).
LEGACY_RENDER_EXPORT int seed_ogr_layer_into_scene(LP3DRENDERDEVICE device,
                                                   SmtScene* scene,
                                                   OGRLayer* layer);

// Open a GeoJSON (or other OGR) file and seed its first layer.
LEGACY_RENDER_EXPORT int seed_geojson_into_scene(LP3DRENDERDEVICE device,
                                                 SmtScene* scene,
                                                 const char* path);

// Resolve china sample map under shared out/data/ (exe → ../data) /
// testing/data.
LEGACY_RENDER_EXPORT int seed_sample_map_into_scene(LP3DRENDERDEVICE device,
                                                    SmtScene* scene);

// Non-owning pointer to the World last filled by seed_* stereo underlay
// (kTerrain + optional mesh). Valid for the process lifetime of this DLL.
LEGACY_RENDER_EXPORT gis::World* map_seeded_world();

}  // namespace render

#endif  // SMT_LEGACY_RENDER_SCENE3D_MAP_TO_SCENE_H_
