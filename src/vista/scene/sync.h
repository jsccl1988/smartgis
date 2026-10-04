// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// World node → GpuInstance CPU copy (no RHI).

#ifndef VISTA_SCENE_MESH_SYNC_H_
#define VISTA_SCENE_MESH_SYNC_H_

#include <vector>

#include "vista/scene/gpu_instance.h"
#include "vista/world/world.h"

namespace vista {
namespace detail {

void copy_world_instances(const vista::World& world,
                          std::vector<GpuInstance>* out);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_SCENE_MESH_SYNC_H_
