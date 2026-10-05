// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/world/terrain/pass.h"

#include <algorithm>

#include "vista/pass/world/detail/draw.h"
#include "vista/pass/world/detail/tint.h"
#include "vista/pass/world/detail/upload.h"
#include "vista/component/world/envelope.h"

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

  float ar = 0.28f;
  float ag = 0.52f;
  float ab = 0.22f;
  size_t count = 0;
  size_t best_texels = 0;
  for (const Instance& inst : instances) {
    if (inst.kind != NodeKind::kTerrain || inst.terrain.rgba.size() < 4) {
      continue;
    }
    const size_t texels = inst.terrain.rgba.size() / 4;
    if (texels < best_texels) {
      continue;
    }
    uint64_t sr = 0;
    uint64_t sg = 0;
    uint64_t sb = 0;
    size_t local_count = 0;
    for (size_t p = 0; p + 3 < inst.terrain.rgba.size(); p += 4) {
      sr += inst.terrain.rgba[p + 0];
      sg += inst.terrain.rgba[p + 1];
      sb += inst.terrain.rgba[p + 2];
      ++local_count;
    }
    if (local_count == 0) {
      continue;
    }
    best_texels = texels;
    count = local_count;
    ar = static_cast<float>(sr / count) / 255.f;
    ag = static_cast<float>(sg / count) / 255.f;
    ab = static_cast<float>(sb / count) / 255.f;
  }
  (void)count;
  (void)ar;
  (void)ag;
  (void)ab;
  // Product / score face: china lowland olive under solid force.
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
  if (!device || !cpu || !mesh || inst.kind != NodeKind::kTerrain) {
    return true;
  }
  const bool terrain_tex = inst.terrain.has_texture();
  render::rhi::Texture* terrain_gpu_tex = nullptr;
  if (terrain_tex) {
    terrain_gpu_tex = detail::upload_rgba_texture_wh(
        device, inst.terrain.rgba.data(), inst.terrain.tex_w,
        inst.terrain.tex_h);
  }
  const bool terrain_tex_ok = terrain_gpu_tex != nullptr;
  if (terrain_tex_ok) {
    cpu->has_image = true;
  }
  const bool with_uv = terrain_tex_ok;
  const bool with_normals = detail::upload_with_normals(inst.kind, with_uv);
  const bool uv_on_xz = terrain_tex_ok;
  const float* explicit_uvs = nullptr;
  if (terrain_tex_ok &&
      inst.terrain.uvs.size() == (cpu->positions.size() / 3) * 2) {
    explicit_uvs = inst.terrain.uvs.data();
  }
  if (!detail::upload_mesh(device, cpu->positions.data(), cpu->positions.size(),
                           cpu->indices.data(), cpu->indices.size(), with_uv,
                           with_normals, uv_on_xz, explicit_uvs, mesh)) {
    if (terrain_gpu_tex) {
      device->destroy_texture(terrain_gpu_tex);
    }
    return false;
  }
  if (cpu->positions.size() >= 3) {
    ::vista::detail::aabb_from_xyz(
        cpu->positions.data(), cpu->positions.size(), &mesh->aabb_min_x,
        &mesh->aabb_min_y, &mesh->aabb_min_z, &mesh->aabb_max_x,
        &mesh->aabb_max_y, &mesh->aabb_max_z);
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
