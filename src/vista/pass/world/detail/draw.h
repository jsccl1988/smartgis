// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Per-NodeKind render-pass recording for WorldPass::record_draws.

#ifndef VISTA_PASS_WORLD_DETAIL_DRAW_H_
#define VISTA_PASS_WORLD_DETAIL_DRAW_H_

#include <cstdint>
#include <vector>

#include "render/programs/programs.h"
#include "render/rhi/rhi.h"
#include "vista/component/world/cull/frustum_aabb.h"
#include "vista/pass/world/gpu_mesh.h"
#include "vista/component/world/world.h"

namespace vista {
namespace detail {

bool mesh_culled(const GpuMesh& mesh, const FrustumPlanes* cull_frustum);

void record_kind(render::rhi::CommandList* list,
                 const render::rhi::RenderPassDesc& pass, uint32_t width,
                 uint32_t height, const std::vector<GpuMesh>& meshes,
                 vista::NodeKind kind, bool* pass_opened,
                 render::rhi::Pipeline* solid, render::rhi::Pipeline* textured,
                 render::rhi::Pipeline* lit_pipeline,
                 render::rhi::Pipeline* lit_textured_pipeline,
                 const render::programs::Light& light,
                 const FrustumPlanes* cull_frustum,
                 const std::vector<uint8_t>* mesh_visible = nullptr,
                 bool force_solid_terrain = false);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_PASS_WORLD_DETAIL_DRAW_H_
