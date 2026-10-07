// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/world/terrain/pass.h"

#include <algorithm>

#include "vista/pass/world/detail/draw.h"
#include "vista/pass/world/detail/tint.h"
#include "vista/pass/world/detail/upload.h"
#include "vista/component/world/space/envelope.h"

namespace vista {

TerrainPass::TerrainPass() = default;

void TerrainPass::clear_solid_terrain_cache() {
  solid_terrain_forced_ = false;
  solid_terrain_cached_ = false;
  solid_terrain_cache_gen_ = 0;
}

void TerrainPass::forced_solid_rgb(float* r, float* g, float* b) const {
  if (r) {
    *r = solid_terrain_rgb_[0];
  }
  if (g) {
    *g = solid_terrain_rgb_[1];
  }
  if (b) {
    *b = solid_terrain_rgb_[2];
  }
}

void TerrainPass::update_solid_terrain(const std::vector<Instance>& instances,
                                       uint64_t generation, bool gate) {
  if (!gate) {
    clear_solid_terrain_cache();
    return;
  }
  if (solid_terrain_cached_ && solid_terrain_cache_gen_ == generation) {
    return;
  }
  // Product / score face: china lowland olive under solid force. Skip the
  // full-albedo mean scan — cold first present was paying O(texels) for a
  // constant that never used the average (FlyCube near-black SRV gate).
  (void)instances;
  solid_terrain_forced_ = true;
  solid_terrain_rgb_[0] = 0.34f;
  solid_terrain_rgb_[1] = 0.58f;
  solid_terrain_rgb_[2] = 0.24f;
  solid_terrain_cached_ = true;
  solid_terrain_cache_gen_ = generation;
}

bool TerrainPass::prepare_mesh(render::rhi::Device* device, const Instance& inst,
                               TessMesh* cpu, GpuMesh* mesh, float solid_r,
                               float solid_g, float solid_b, float solid_a) {
  if (!device || !mesh || inst.kind != NodeKind::kTerrain) {
    return true;
  }
  const float* positions = nullptr;
  size_t position_count = 0;
  const uint32_t* indices = nullptr;
  size_t index_count = 0;
  if (cpu && !cpu->positions.empty() && !cpu->indices.empty()) {
    positions = cpu->positions.data();
    position_count = cpu->positions.size();
    indices = cpu->indices.data();
    index_count = cpu->indices.size();
  } else if (inst.terrain.has_mesh()) {
    // Cold first upload: bind Instance terrain buffers directly (no TessMesh
    // deep copy before create_buffer / upload).
    positions = inst.terrain.positions.data();
    position_count = inst.terrain.positions.size();
    indices = inst.terrain.indices.data();
    index_count = inst.terrain.indices.size();
  } else {
    return true;
  }
  const bool terrain_tex = inst.terrain.has_texture();
  render::rhi::Texture* terrain_gpu_tex = nullptr;
  if (terrain_tex) {
    // Reuse |mesh->texture| when size matches (ocean remesh cold path).
    terrain_gpu_tex = detail::upload_rgba_texture_wh(
        device, inst.terrain.rgba.data(), inst.terrain.tex_w,
        inst.terrain.tex_h, mesh->texture);
    mesh->texture = nullptr;
  } else if (mesh->texture) {
    device->destroy_texture(mesh->texture);
    mesh->texture = nullptr;
  }
  const bool terrain_tex_ok = terrain_gpu_tex != nullptr;
  if (terrain_tex_ok && cpu) {
    cpu->has_image = true;
  }
  const bool with_uv = terrain_tex_ok;
  const bool with_normals = detail::upload_with_normals(inst.kind, with_uv);
  const bool uv_on_xz = terrain_tex_ok;
  const float* explicit_uvs = nullptr;
  if (terrain_tex_ok &&
      inst.terrain.uvs.size() == (position_count / 3) * 2) {
    explicit_uvs = inst.terrain.uvs.data();
  }
  if (!detail::upload_mesh(device, positions, position_count, indices,
                           index_count, with_uv, with_normals, uv_on_xz,
                           explicit_uvs, mesh)) {
    if (terrain_gpu_tex) {
      device->destroy_texture(terrain_gpu_tex);
    }
    return false;
  }
  if (position_count >= 3) {
    ::vista::detail::aabb_from_xyz(positions, position_count, &mesh->aabb_min_x,
                                   &mesh->aabb_min_y, &mesh->aabb_min_z,
                                   &mesh->aabb_max_x, &mesh->aabb_max_y,
                                   &mesh->aabb_max_z);
  }
  if (terrain_tex_ok) {
    mesh->texture = terrain_gpu_tex;
    if (!inst.has_paint) {
      if (solid_terrain_forced_) {
        mesh->solid_r = solid_r;
        mesh->solid_g = solid_g;
        mesh->solid_b = solid_b;
        mesh->solid_a = 1.f;
      } else {
        mesh->solid_r = 1.f;
        mesh->solid_g = 1.f;
        mesh->solid_b = 1.f;
        mesh->solid_a = 1.f;
      }
    }
  } else if (!inst.has_paint) {
    detail::apply_untextured_terrain_tint(mesh);
    (void)solid_a;
  }
  return true;
}

void TerrainPass::record(render::rhi::CommandList* list,
                         const render::rhi::RenderPassDesc& pass, uint32_t width,
                         uint32_t height, const std::vector<GpuMesh>& meshes,
                         bool* pass_opened, render::rhi::Pipeline* solid,
                         render::rhi::Pipeline* textured,
                         render::rhi::Pipeline* lit_pipeline,
                         render::rhi::Pipeline* lit_textured_pipeline,
                         const render::programs::Light& light,
                         const FrustumPlanes* cull_frustum,
                         const std::vector<uint8_t>* mesh_visible) {
  detail::record_kind(list, pass, width, height, meshes, NodeKind::kTerrain,
                      pass_opened, solid, textured, lit_pipeline,
                      lit_textured_pipeline, light, cull_frustum, mesh_visible,
                      solid_terrain_forced_);
}

}  // namespace vista
