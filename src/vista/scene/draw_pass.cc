// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/scene/draw_pass.h"

#include "vista/scene/cull/prep_cull.h"
#include "vista/scene/upload.h"

#include <cstddef>

namespace vista {
namespace detail {

bool mesh_culled(const GpuMesh& mesh, const FrustumPlanes* cull_frustum) {
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
                 uint32_t height, const std::vector<GpuMesh>& meshes,
                 vista::NodeKind kind, bool* pass_opened,
                 render::rhi::Pipeline* solid, render::rhi::Pipeline* textured,
                 render::rhi::Pipeline* lit_pipeline,
                 render::rhi::Pipeline* lit_textured_pipeline,
                 const render::programs::Light& light,
                 const FrustumPlanes* cull_frustum,
                 const std::vector<uint8_t>* mesh_visible,
                 bool force_solid_terrain) {
  const bool use_visible =
      mesh_visible && mesh_visible->size() == meshes.size();
  auto is_drawn = [&](size_t i, const GpuMesh& mesh) {
    if (mesh.kind != kind || mesh.index_count == 0 || !mesh.vertex ||
        !mesh.index) {
      return false;
    }
    if (use_visible) {
      return (*mesh_visible)[i] != 0;
    }
    return !mesh_culled(mesh, cull_frustum);
  };

  bool any = false;
  for (size_t i = 0; i < meshes.size(); ++i) {
    if (is_drawn(i, meshes[i])) {
      any = true;
      break;
    }
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
  bool light_bound = false;
  auto bind_light = [&]() {
    if (!light_bound) {
      list->set_constants(render::programs::kLightSlot, &light,
                          static_cast<uint32_t>(sizeof(light)));
      light_bound = true;
    }
  };
  for (size_t i = 0; i < meshes.size(); ++i) {
    const auto& mesh = meshes[i];
    if (!is_drawn(i, mesh)) {
      continue;
    }
    const render::programs::Color color{mesh.solid_r, mesh.solid_g, mesh.solid_b,
                                mesh.solid_a};
    if (mesh.texture && !force_solid_terrain && lit && lit_textured_pipeline &&
        kind == vista::NodeKind::kTerrain) {
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
               (kind == vista::NodeKind::kTerrain ||
                kind == vista::NodeKind::kPointCloud) &&
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
}  // namespace vista
