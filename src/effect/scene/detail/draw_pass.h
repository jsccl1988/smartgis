// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Per-NodeKind render-pass recording for GpuScene::record_draws.

#ifndef EFFECT_SCENE_DETAIL_DRAW_PASS_H_
#define EFFECT_SCENE_DETAIL_DRAW_PASS_H_

#include <cstdint>
#include <vector>

#include "gis/vista/world/world.h"
#include "render/programs/programs.h"
#include "render/rhi/rhi.h"
#include "effect/scene/frustum_aabb.h"
#include "effect/scene/scene.h"

namespace effect {
namespace scene {
namespace detail {

bool frustum_cull_enabled();

bool mesh_culled(const GpuScene::GpuMesh& mesh,
                 const FrustumPlanes* cull_frustum);

// Opens one render pass (or loads into an already-opened clear) and draws
// every mesh of |kind|. Updates *pass_opened when a pass actually begins.
void record_kind(render::rhi::CommandList* list,
                 const render::rhi::RenderPassDesc& pass, uint32_t width,
                 uint32_t height, const std::vector<GpuScene::GpuMesh>& meshes,
                 gis::NodeKind kind, bool* pass_opened,
                 render::rhi::Pipeline* solid, render::rhi::Pipeline* textured,
                 render::rhi::Pipeline* lit_pipeline,
                 render::rhi::Pipeline* lit_textured_pipeline,
                 const render::programs::Light& light,
                 const FrustumPlanes* cull_frustum);

}  // namespace detail
}  // namespace scene
}  // namespace effect

#endif  // EFFECT_SCENE_DETAIL_DRAW_PASS_H_
