// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Cpu frustum prep for GpuScene: workers write visible[] only (no Device / CL).

#ifndef EFFECT_SCENE_DETAIL_PREP_CULL_H_
#define EFFECT_SCENE_DETAIL_PREP_CULL_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vista/scene/frustum_aabb.h"
#include "vista/scene/scene.h"
#include "vista/vista_export.h"

namespace vista {
namespace detail {

// SMT_GPUSCENE_PREP_PARALLEL env alone (ignores frustum cull).
// Product default / unset / 0 / false → 1 (serial). Explicit opt-in → clamp 2–4.
VISTA_EXPORT int prep_parallel_requested_workers();

// Effective worker count for this frame. Always 1 unless frustum cull is
// active AND SMT_GPUSCENE_PREP_PARALLEL opted in (prep alone never fans out).
VISTA_EXPORT int prep_parallel_effective_workers(bool frustum_cull_active);

// Fill visible[i] for every mesh. When cull is off or |frustum| is null,
// all entries stay 1 and no worker pool is touched — even if prep_par=1.
// When cull is on: serial or parallel_for AABB tests; workers never touch RHI.
VISTA_EXPORT void prep_cull_meshes(const std::vector<GpuScene::GpuMesh>& meshes,
                                   const FrustumPlanes* frustum,
                                   std::vector<uint8_t>* visible);

}  // namespace detail
}  // namespace vista

#endif  // EFFECT_SCENE_DETAIL_PREP_CULL_H_
