// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/scene/detail/draw_pass.h"

#include "effect/scene/detail/upload.h"

#include <cstdlib>

namespace effect {
namespace scene {
namespace detail {

bool frustum_cull_enabled() {
  // Default off for Scene3d DEM: geographic→orbit AABB mismatches historically
  // culled the whole terrain (blank navy clear on interactive HWND while
  // 640x480 showcase still drew). Opt in with SMT_SCENE3D_FRUSTUM_CULL=1.
  const char* on = std::getenv("SMT_SCENE3D_FRUSTUM_CULL");
  if (on && on[0] == '1' && on[1] == '\0') {
    return true;
  }
  const char* e = std::getenv("SMT_SCENE3D_NO_CULL");
  if (e && e[0] == '1' && e[1] == '\0') {
    return false;
  }
  return false;
}

bool mesh_culled(const GpuScene::GpuMesh& mesh,
                 const FrustumPlanes* cull_frustum) {
  if (!frustum_cull_enabled() || !cull_frustum) {
    return false;
  }
  return !aabb_intersects_frustum(mesh.aabb_min_x, mesh.aabb_min_y,
                                  mesh.aabb_min_z, mesh.aabb_max_x,
                                  mesh.aabb_max_y, mesh.aabb_max_z,
                                  *cull_frustum);
}

void record_kind(render::rhi::CommandList* list,
                 const render::rhi::RenderPassDesc& pass, uint32_t width,
                 uint32_t height, const std::vector<GpuScene::GpuMesh>& meshes,
                 gis::NodeKind kind, bool* pass_opened,
                 render::rhi::Pipeline* solid, render::rhi::Pipeline* textured,
                 render::rhi::Pipeline* lit_pipeline,
                 render::rhi::Pipeline* lit_textured_pipeline,
                 const render::programs::Light& light,
                 const FrustumPlanes* cull_frustum) {
  bool any = false;
  for (const auto& mesh : meshes) {
    if (mesh.kind != kind || mesh.index_count == 0 || !mesh.vertex ||
        !mesh.index) {
      continue;
    }
    if (mesh_culled(mesh, cull_frustum)) {
      continue;
    }
    any = true;
    break;
  }
  if (!any) {
    return;
  }
  render::rhi::RenderPassDesc local = pass;
  if (pass_opened && *pass_opened) {
    // Subsequent kinds share the frame; do not replace the clear color/depth.
    local.load_op = render::rhi::ColorLoadOp::kLoad;
    if (local.enable_depth) {
      local.depth_load_op = render::rhi::DepthLoadOp::kLoad;
    }
  }
  list->begin_render_pass(local);
  if (pass_opened) {
    *pass_opened = true;
  }
  list->set_viewport(0, 0, static_cast<float>(width), static_cast<float>(height),
                     0, 1);
  if (pass.enable_depth) {
    list->set_depth_mode(render::rhi::DepthMode::kWrite);
  }
  const bool lit = is_lit_kind(kind);
  const bool lit_terrain = want_lit_terrain();
  const bool force_solid_terrain = []() {
    if (const char* e = std::getenv("SMT_SCENE3D_SOLID_TERRAIN")) {
      return e[0] == '1' && e[1] == '\0';
    }
    return false;
  }();
  bool light_bound = false;
  auto bind_light = [&]() {
    if (!light_bound) {
      list->set_constants(render::programs::kLightSlot, &light,
                          static_cast<uint32_t>(sizeof(light)));
      light_bound = true;
    }
  };
  for (const auto& mesh : meshes) {
    if (mesh.kind != kind || mesh.index_count == 0 || !mesh.vertex ||
        !mesh.index) {
      continue;
    }
    if (mesh_culled(mesh, cull_frustum)) {
      continue;
    }
    const render::programs::Color color{mesh.solid_r, mesh.solid_g, mesh.solid_b,
                                mesh.solid_a};
    if (mesh.texture && !force_solid_terrain && lit && lit_textured_pipeline &&
        kind == gis::NodeKind::kTerrain) {
      // DEM hypsometric / draped imagery. Prefer unlit textured so authored
      // bake RGB reaches the swapchain.
      // Write DEM depth so post-opaque ocean/cloud/fog depth-test correctly.
      // (Ocean must record *after* this draw — see Scene3dGpuPresent::present.)
      if (pass.enable_depth) {
        list->set_depth_mode(render::rhi::DepthMode::kWrite);
      }
      if (lit_terrain) {
        list->set_pipeline(lit_textured_pipeline);
        list->bind_texture(mesh.texture, render::programs::kTextureSlot);
        bind_light();
      } else {
        list->set_pipeline(textured);
        list->bind_texture(mesh.texture, render::programs::kTextureSlot);
      }
      list->set_constants(render::programs::kColorSlot, &color,
                          static_cast<uint32_t>(sizeof(color)));
    } else if (mesh.texture && !force_solid_terrain) {
      list->set_pipeline(textured);
      list->bind_texture(mesh.texture, render::programs::kTextureSlot);
      // kPsTextured multiplies sample by ColorCB tint (map2d opacity path).
      list->set_constants(render::programs::kColorSlot, &color,
                          static_cast<uint32_t>(sizeof(color)));
    } else if (lit &&
               (kind == gis::NodeKind::kTerrain ||
                kind == gis::NodeKind::kPointCloud) &&
               !lit_terrain) {
      // Untextured terrain paint (mine TIN) + colored point beads: solid PS.
      list->set_pipeline(solid);
      list->set_constants(render::programs::kColorSlot, &color,
                          static_cast<uint32_t>(sizeof(color)));
    } else if (lit) {
      // Models / tilesets (and terrain when SMT_SCENE3D_LIT_TERRAIN=1).
      list->set_pipeline(lit_pipeline);
      bind_light();
      list->set_constants(render::programs::kColorSlot, &color,
                          static_cast<uint32_t>(sizeof(color)));
    } else {
      list->set_pipeline(solid);
      list->set_constants(render::programs::kColorSlot, &color,
                          static_cast<uint32_t>(sizeof(color)));
    }
    list->bind_vertex_buffer(mesh.vertex, 0,
                            mesh.stride ? mesh.stride : kPositionStride);
    list->bind_index_buffer(mesh.index, 0);
    list->draw_indexed(mesh.index_count, 1, 0, 0, 0);
  }
  list->end_render_pass();
}

}  // namespace detail
}  // namespace scene
}  // namespace effect
