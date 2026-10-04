// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_MAP_TO_SCENE_H_
#define SCENIC_SCENE3D_MAP_TO_SCENE_H_

#include "vista/world/world.h"
#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/camera/camera.h"
#include "scenic/render/rhi3d/public/device/render_device.h"
#include "vista/terrain/dem/dem_height_field.h"
#include "scenic/scene3d/scene/scene.h"
#include "scenic/scene3d/scene/vertex3d.h"

class OGRLayer;

namespace scenic {
namespace detail {

// 400x300-class leftover 3D viewport: sane perspective (never zNear=0).
SCENIC_IMPL_EXPORT void apply_view3d_viewport(Viewport3D* vp, ulong width,
                                                ulong height);

// Leftover Y-up pose that frames |aabb| (X=-lon, Y=elev, Z=lat).
// Empty AABB falls back to origin + span 40 (legacy cube).
SCENIC_IMPL_EXPORT void leftover_frame_pose(const Aabb& aabb, Vector3* eye,
                                              Vector3* target, float* span);

// Scene DEM leftover AABB (X=-lon, height→Y, lat→Z). False when no DEM seeded.
// Uses the last successful seed's framing cache (does not block another scene).
SCENIC_IMPL_EXPORT bool leftover_dem_aabb(Aabb* out);

// True when |eye| is still the leftover origin pose (not over the DEM).
SCENIC_IMPL_EXPORT bool leftover_eye_misses_dem(const Vector3& eye);

// True after any successful stereo DEM seed (last frame cache). Not a sticky
// "already have DEM → skip seeding this Scene" gate.
SCENIC_IMPL_EXPORT bool leftover_has_scene_dem();

// Place a perspective camera so the DEM (preferred) or scene AABB fills the
// frustum. Always raises zFar from the framed span.
SCENIC_IMPL_EXPORT void frame_persp_camera_to_aabb(PerspCamera* camera,
                                                     Viewport3D* vp,
                                                     const ::base::Aabb& aabb);

// Lift an OGR layer (china_plp regions/lines/points/labels) into leftover 3D
// objects. Polygons/lines drape on DemHeightField; place-names go to labels.
// Returns the number of objects added. Safe when style is missing on features.
// SP4: stereo DEM also seeds map_seeded_world() via
// seed_dem_height_field_into_world (envelope + CPU mesh for GpuScene).
SCENIC_IMPL_EXPORT int seed_ogr_layer_into_scene(LP3DRENDERDEVICE device,
                                                   Scene* scene,
                                                   OGRLayer* layer);

// Open a GeoJSON (or other OGR) file and seed its first layer.
SCENIC_IMPL_EXPORT int seed_geojson_into_scene(LP3DRENDERDEVICE device,
                                                 Scene* scene,
                                                 const char* path);

// Resolve china sample map under shared out/data/ (exe → ../data) /
// testing/data.
SCENIC_IMPL_EXPORT int seed_sample_map_into_scene(LP3DRENDERDEVICE device,
                                                    Scene* scene);

// Per-object leftover showcase seed for --scene3d-showcase <mode>.
// Modes: china (default) | terrain | cube | sphere | water | pointcloud |
// northarray. Mesh modes clear the DEM framing cache so orbit frames the
// object AABB. Returns objects added (northarray returns 1 with a synthetic
// AABB; compass HUD is created in Scene::Setup).
SCENIC_IMPL_EXPORT int seed_showcase_mode_into_scene(LP3DRENDERDEVICE device,
                                                       Scene* scene,
                                                       const char* mode);

// SMT_SCENE3D_SHOWCASE_MODE env, or "china" when unset / empty.
SCENIC_IMPL_EXPORT const char* showcase_mode_from_env();

// Drop DEM framing cache (mesh showcase must not prefer leftover china AABB).
SCENIC_IMPL_EXPORT void clear_leftover_dem_frame();

// Non-owning pointer to the World last filled by seed_* stereo underlay
// (kTerrain + optional mesh). Valid for the process lifetime of this DLL.
SCENIC_IMPL_EXPORT vista::World* map_seeded_world();

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_SCENE3D_MAP_TO_SCENE_H_
