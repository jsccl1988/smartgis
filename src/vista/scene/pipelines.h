// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Which GpuInstance kinds need lit PSOs (CPU, no Device).

#ifndef VISTA_SCENE_MESH_PIPELINES_H_
#define VISTA_SCENE_MESH_PIPELINES_H_

#include <vector>

#include "vista/scene/gpu_instance.h"

namespace vista {
namespace detail {

bool want_model_lit(const std::vector<GpuInstance>& instances);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_SCENE_MESH_PIPELINES_H_
