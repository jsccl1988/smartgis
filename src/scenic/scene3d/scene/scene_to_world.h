// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_SCENE_TO_WORLD_H_
#define SCENIC_SCENE3D_SCENE_TO_WORLD_H_

#include <cstddef>

#include "vista/component/world/world.h"
#include "vista/component/world/coord.h"
#include "scenic/render/scenic_impl_export.h"
#include "scenic/scene3d/scene/scene.h"

namespace scenic {
namespace detail {

// Mirror every Object3d AABB into |world| as kEmpty nodes (keeps existing
// kTerrain / other kinds). Hot path for create_octree switch.
// Pure leftover Y-up ↔ GIS helpers live in vista/component/world/coord.h.
SCENIC_IMPL_EXPORT size_t
seed_smt_scene_aabbs_into_world(vista::World* world, const Scene* scene);

// Non-owning World mirror for create_octree (map_to_scene sets this
// to map_seeded_world()). Null = no World update on octree rebuild.
SCENIC_IMPL_EXPORT void set_smt_scene_world_mirror(vista::World* world);
SCENIC_IMPL_EXPORT vista::World* scene_world_mirror();

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_SCENE3D_SCENE_TO_WORLD_H_
