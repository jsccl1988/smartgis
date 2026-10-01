// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_SCENE3D_SCENE_TO_WORLD_H_
#define SMT_LEGACY_RENDER_SCENE3D_SCENE_TO_WORLD_H_

#include <cstddef>

#include "gis/vista/world/world.h"
#include "legacy/gis/vista/coord.h"
#include "legacy/render/legacy_render_export.h"
#include "legacy/render/scene3d/scene/scene.h"

namespace render {

// Mirror every Smt3DObject AABB into |world| as kEmpty nodes (keeps existing
// kTerrain / other kinds). Hot path for CreateOctTreeSceneMgr switch.
// Pure leftover Y-up ↔ GIS helpers live in legacy/gis/vista/coord.h.
LEGACY_RENDER_EXPORT size_t
seed_smt_scene_aabbs_into_world(gis::World* world, const SmtScene* scene);

// Non-owning World mirror for CreateOctTreeSceneMgr (map_to_scene sets this
// to map_seeded_world()). Null = no World update on octree rebuild.
LEGACY_RENDER_EXPORT void set_smt_scene_world_mirror(gis::World* world);
LEGACY_RENDER_EXPORT gis::World* smt_scene_world_mirror();

}  // namespace render

#endif  // SMT_LEGACY_RENDER_SCENE3D_SCENE_TO_WORLD_H_
