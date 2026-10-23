// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/world/detail/draw.h"

#include "base/core/log.h"
#include "vista/component/world/space/cull/prep_cull.h"
#include "vista/pass/world/detail/upload.h"

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
  if (kind == vista::NodeKind::kTerrain) {
    static int terrain_draw_log = 0;
    if (terrain_draw_log < 6) {
      ++terrain_draw_log;
      size_t terrain_meshes = 0;
      size_t drawn = 0;
      uint32_t indices = 0;
      int textured = 0;
      for (size_t i = 0; i < meshes.size(); ++i) {
        const GpuMesh& mesh = meshes[i];
        if (mesh.kind != kind) {
          continue;
        }
        ++terrain_meshes;
        if (is_drawn(i, mesh)) {
          ++drawn;
          indices += mesh.index_count;
          if (mesh.texture) {
            textured = 1;
          }
        }
      }
      LOGGING(LOG_INFO,
              "scene3d.terrain.draw meshes=%zu drawn=%zu idx=%u tex=%d "
              "visible_list=%d",
              terrain_meshes, drawn, indices, textured, use_visible ? 1 : 0);
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
  // Solid-forced DEM must still kWrite on depth-attached opaque passes.
  // kDisabled needs a depth_off_ds_ PSO variant; solid_pipeline_desc does not
  // compile one, so FlyCube skips the draw (navy clear + translucent ghost on
  // atmosphere.full / large FlyCube HWND). Ocean stays skipped under the
  // albedo gate — DEM depth is what post sky/cloud expect.
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
    // Albedo gate: FlyCube textured SRVs sample near-black after ocean/sky heap
    // churn. Solid PSO + olive ColorCB is the score face for china DEM. Plugin
    // overlay keep path uploads atlas + identity white ColorCB — only then skip
    // the olive force (a stale DEM SRV pointer must not bind under olive).
    // Position-only solid PSO stays on kWrite (depth-off leaves navy clear).
    const bool overlay_kept_tex =
        force_solid_terrain && kind == vista::NodeKind::kTerrain &&
        mesh.texture && mesh.solid_r >= 0.95f && mesh.solid_g >= 0.95f &&
        mesh.solid_b >= 0.95f && mesh.solid_a >= 0.95f;
    if (force_solid_terrain && kind == vista::NodeKind::kTerrain && solid &&
        !overlay_kept_tex) {
      if (pass.enable_depth) {
        list->set_depth_mode(render::rhi::DepthMode::kWrite);
      }
      list->set_pipeline(solid);
      list->set_constants(render::programs::kColorSlot, &color,
                          static_cast<uint32_t>(sizeof(color)));
    } else if (mesh.texture && lit && lit_textured_pipeline &&
        kind == vista::NodeKind::kTerrain) {
      // DEM hypsometric / draped imagery / olive gate. Prefer unlit textured so
      // authored bake RGB reaches the swapchain. Depth-off: kWrite on this
      // pass rasterizes nothing (clear stays 0,0.2,0.4).
      if (pass.enable_depth) {
        list->set_depth_mode(render::rhi::DepthMode::kDisabled);
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
    } else if (mesh.texture) {
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
      // Models / tilesets (and terrain when SCENE3D_LIT_TERRAIN=1).
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
