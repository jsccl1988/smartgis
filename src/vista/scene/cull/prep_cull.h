// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Cpu frustum prep for GpuScene: workers write visible[] only (no Device / CL).

#ifndef VISTA_SCENE_CULL_PREP_CULL_H_
#define VISTA_SCENE_CULL_PREP_CULL_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vista/scene/cull/frustum_aabb.h"
#include "vista/scene/cull/mesh_cull.h"
#include "vista/scene/index/aabb_octree.h"
#include "vista/vista_export.h"

namespace vista {
namespace detail {

VISTA_EXPORT bool frustum_cull_enabled();

// SMT_GPUSCENE_PREP_PARALLEL / --gpuscene-prep-parallel alone (ignores frustum).
// Product default / unset / 0 / false → 1 (serial). Explicit opt-in → clamp 2–4.
VISTA_EXPORT int prep_parallel_requested_workers();

// Effective worker count for this frame. Always 1 unless frustum cull is
// active AND prep-parallel opted in (prep alone never fans out).
VISTA_EXPORT int prep_parallel_effective_workers(bool frustum_cull_active);

// Fill visible[i] for every mesh. When cull is off or |frustum| is null,
// all entries stay 1 and no worker pool is touched — even if prep_par=1.
// When cull is on: optional frame-local octree, then AABB tests.
// |index| unused (frame-local tree); |view|/|proj| enable octree radius query.
VISTA_EXPORT void prep_cull_meshes(const std::vector<MeshCullItem>& meshes,
                                   const FrustumPlanes* frustum,
                                   std::vector<uint8_t>* visible,
                                   AabbOctree* index = nullptr,
                                   const float* view = nullptr,
                                   const float* proj = nullptr);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_SCENE_CULL_PREP_CULL_H_
