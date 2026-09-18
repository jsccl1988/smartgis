// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_SCENE3D_MAP_TO_SCENE_H_
#define SMT_LEGACY_RENDER_SCENE3D_MAP_TO_SCENE_H_

#include "legacy/render/render3d/3drenderdevice.h"
#include "legacy/render/render3d/camera.h"
#include "legacy/render/scene3d/bl3d_bas_struct.h"
#include "legacy/render/scene3d/bl3d_scene.h"

class OGRLayer;

namespace render {

// 400x300-class leftover 3D viewport: sane perspective (never zNear=0).
SCENE3D_EXPORT_API void apply_view3d_viewport(Viewport3D* vp,
                                              ulong width,
                                              ulong height);

// Place a perspective camera so the scene AABB fills the frustum.
SCENE3D_EXPORT_API void frame_persp_camera_to_aabb(SmtPerspCamera* camera,
                                                   Viewport3D* vp,
                                                   const Aabb& aabb);

// Lift an OGR layer (china_plp regions/lines/points/labels) into leftover 3D
// objects. Label points become 3D dots (2D GDI draws them as text).
// Returns the number of objects added. Safe when style is missing on features.
SCENE3D_EXPORT_API int seed_ogr_layer_into_scene(LP3DRENDERDEVICE device,
                                                 SmtScene* scene,
                                                 OGRLayer* layer);

// Open a GeoJSON (or other OGR) file and seed its first layer.
SCENE3D_EXPORT_API int seed_geojson_into_scene(LP3DRENDERDEVICE device,
                                               SmtScene* scene,
                                               const char* path);

// Resolve china_plp.geojson next to the exe / testing/data and seed it.
SCENE3D_EXPORT_API int seed_sample_map_into_scene(LP3DRENDERDEVICE device,
                                                  SmtScene* scene);

}  // namespace render

#endif  // SMT_LEGACY_RENDER_SCENE3D_MAP_TO_SCENE_H_
