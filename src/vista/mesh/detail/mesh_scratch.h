// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_MESH_DETAIL_MESH_SCRATCH_H_
#define VISTA_MESH_DETAIL_MESH_SCRATCH_H_

#include <vector>

#include "base/memory/object_pool.h"
#include "vista/mesh/detail/mesh_types.h"

namespace vista {
namespace detail {

// Per-worker scratch pools (tessellate_* runs under parallel_for).
base::ObjectPool<std::vector<PolyPt>>& poly_pt_vec_pool();
base::ObjectPool<std::vector<Vec2>>& vec2_vec_pool();
base::ObjectPool<std::vector<std::vector<PolyPt>>>& dash_vec_pool();

}  // namespace detail
}  // namespace vista

#endif  // VISTA_MESH_DETAIL_MESH_SCRATCH_H_
