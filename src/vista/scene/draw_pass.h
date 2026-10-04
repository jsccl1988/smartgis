// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Per-NodeKind render-pass recording for GpuScene::record_draws.

#ifndef VISTA_SCENE_DETAIL_DRAW_PASS_H_
#define VISTA_SCENE_DETAIL_DRAW_PASS_H_

#include <cstdint>
#include <vector>

#include "render/programs/programs.h"
#include "render/rhi/rhi.h"
#include "vista/scene/cull/frustum_aabb.h"
#include "vista/scene/gpu_mesh.h"
#include "vista/world/world.h"

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

#endif  // VISTA_SCENE_DETAIL_DRAW_PASS_H_
