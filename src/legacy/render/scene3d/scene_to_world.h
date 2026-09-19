// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_SCENE3D_SCENE_TO_WORLD_H_
#define SMT_LEGACY_RENDER_SCENE3D_SCENE_TO_WORLD_H_

#include <cstddef>

#include "gis/world/scene.h"
#include "legacy/render/scene3d/bl3d_scene.h"
#include "render/math/aabb.h"

namespace render {

// Convert leftover Y-up corner (X=-lon, Y=elev, Z=lat) into GIS World envelope
// (min_x/min_y/min_z)–(max_x/max_y/max_z) where horizontal is lon/lat and
// vertical elev lands in Z. Pure logic — unit-testable without SmtScene.
SCENE3D_EXPORT_API void leftover_yup_to_gis(double x0, double elev0,
                                            double lat0, double x1,
                                            double elev1, double lat1,
                                            double* min_x, double* min_y,
                                            double* min_z, double* max_x,
                                            double* max_y, double* max_z);

SCENE3D_EXPORT_API void leftover_aabb_to_gis(const Aabb& aabb, double* min_x,
                                             double* min_y, double* min_z,
                                             double* max_x, double* max_y,
                                             double* max_z);

// Attach one GIS AABB as an empty spatial placeholder (octree mirror seam).
SCENE3D_EXPORT_API gis::Node* attach_gis_aabb(gis::World* world,
                                              const char* name, double min_x,
                                              double min_y, double min_z,
                                              double max_x, double max_y,
                                              double max_z);

// Mirror every Smt3DObject AABB into |world| as kEmpty nodes (keeps existing
// kTerrain / other kinds). Hot path for CreateOctTreeSceneMgr switch.
SCENE3D_EXPORT_API size_t seed_smt_scene_aabbs_into_world(gis::World* world,
                                                          const SmtScene* scene);

// Non-owning World mirror for CreateOctTreeSceneMgr (map_to_scene sets this
// to map_seeded_world()). Null = no World update on octree rebuild.
SCENE3D_EXPORT_API void set_smt_scene_world_mirror(gis::World* world);
SCENE3D_EXPORT_API gis::World* smt_scene_world_mirror();

}  // namespace render

#endif  // SMT_LEGACY_RENDER_SCENE3D_SCENE_TO_WORLD_H_
