// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/scene/scene.h"

#include "base/core/log.h"
#include "gis/map/map_layer.h"
#include "vista/assets/model/model.h"
#include "vista/world/terrain/mesh/tessellate.h"
#include "vista/scene/detail/paint.h"
#include "vista/scene/detail/tessellate.h"
#include "vista/scene/detail/upload.h"

#include "ogrsf_frmts.h"

namespace vista {

void GpuScene::clear_meshes() {
  if (upload_device_) {
    for (GpuMesh& mesh : meshes_) {
      upload_device_->destroy_buffer(mesh.vertex);
      upload_device_->destroy_buffer(mesh.index);
      upload_device_->destroy_texture(mesh.texture);
    }
  }
  meshes_.clear();
  upload_device_ = nullptr;
  upload_width_ = 0;
  upload_height_ = 0;
}

void GpuScene::resolve_view_envelope(uint32_t width, uint32_t height,
                                     double* min_x, double* min_y,
                                     double* max_x, double* max_y) const {
  double minx = 0;
  double miny = 0;
  double maxx = 1;
  double maxy = 1;
  bool have_box = false;
  if (view_ortho_set_) {
    minx = view_min_x_;
    miny = view_min_y_;
    maxx = view_max_x_;
    maxy = view_max_y_;
    have_box = true;
  } else {
    for (const GpuInstance& inst : instances_) {
      if (inst.kind != vista::NodeKind::kRasterLayer &&
          inst.kind != vista::NodeKind::kVectorLayer) {
        continue;
      }
      if (!have_box) {
        minx = inst.min_x;
        miny = inst.min_y;
        maxx = inst.max_x;
        maxy = inst.max_y;
        have_box = true;
      } else {
        if (inst.min_x < minx) {
          minx = inst.min_x;
        }
        if (inst.min_y < miny) {
          miny = inst.min_y;
        }
        if (inst.max_x > maxx) {
          maxx = inst.max_x;
        }
        if (inst.max_y > maxy) {
          maxy = inst.max_y;
        }
      }
    }
  }
  if (!have_box) {
    minx = 0;
    miny = 0;
    maxx = static_cast<double>(width);
    maxy = static_cast<double>(height);
  }
  if (maxx - minx < 1e-6) {
    maxx = minx + 1;
  }
  if (maxy - miny < 1e-6) {
    maxy = miny + 1;
  }
  if (min_x) {
    *min_x = minx;
  }
  if (min_y) {
    *min_y = miny;
  }
  if (max_x) {
    *max_x = maxx;
  }
  if (max_y) {
    *max_y = maxy;
  }
}

bool GpuScene::rebuild_meshes(render::rhi::Device* device, uint32_t width,
                             uint32_t height) {
  LOGGING(LOG_INFO, "GpuScene.rebuild_meshes size=%ux%u instances=%zu", width,
          height, instances_.size());
  if (!device || width == 0 || height == 0) {
    clear_meshes();
    return false;
  }
  // Keep prior GPU buffers so upload_mesh can reuse matching VB/IB sizes
  // instead of destroy+create on every remesh.
  std::vector<GpuMesh> prev_meshes = std::move(meshes_);
  meshes_.clear();
  upload_device_ = device;

  double env_min_x = 0;
  double env_min_y = 0;
  double env_max_x = 1;
  double env_max_y = 1;
  resolve_view_envelope(width, height, &env_min_x, &env_min_y, &env_max_x,
                        &env_max_y);
  const double world_units_per_pixel =
      (env_max_x - env_min_x) / static_cast<double>(width);

  meshes_.reserve(instances_.size());
  size_t reuse_slot = 0;
  for (const GpuInstance& inst : instances_) {
    GpuMesh mesh;
    mesh.kind = inst.kind;
    mesh.vertex = nullptr;
    mesh.index = nullptr;
    mesh.texture = nullptr;
    mesh.index_count = 0;
    mesh.stride = detail::kPositionStride;
    if (reuse_slot < prev_meshes.size()) {
      mesh.vertex = prev_meshes[reuse_slot].vertex;
      mesh.index = prev_meshes[reuse_slot].index;
      prev_meshes[reuse_slot].vertex = nullptr;
      prev_meshes[reuse_slot].index = nullptr;
      if (prev_meshes[reuse_slot].texture) {
        device->destroy_texture(prev_meshes[reuse_slot].texture);
        prev_meshes[reuse_slot].texture = nullptr;
      }
    }
    ++reuse_slot;
    mesh.solid_r = solid_r_;
    mesh.solid_g = solid_g_;
    mesh.solid_b = solid_b_;
    mesh.solid_a = solid_a_;
    mesh.line_width = 1.f;
    mesh.circle_radius = 5.f;
    mesh.aabb_min_x = static_cast<float>(inst.min_x);
    mesh.aabb_min_y = static_cast<float>(inst.min_y);
    mesh.aabb_min_z = static_cast<float>(inst.min_z);
    mesh.aabb_max_x = static_cast<float>(inst.max_x);
    mesh.aabb_max_y = static_cast<float>(inst.max_y);
    mesh.aabb_max_z = static_cast<float>(inst.max_z);
    if (inst.has_paint) {
      detail::apply_paint_scalars(inst.paint, &mesh);
    }
    const bool want_symbol =
        inst.has_paint && inst.paint.has_symbol &&
        (!inst.paint.symbol.bytes.empty() || !inst.paint.symbol.path.empty());

    // P1: one GPU mesh per point-cloud chunk (AABB frustum cull in record).
    if (inst.kind == vista::NodeKind::kPointCloud &&
        !inst.point_chunks.empty() && !inst.point_positions.empty() &&
        (inst.point_positions.size() % 3) == 0) {
      const bool with_normals =
          detail::upload_with_normals(inst.kind, /*with_uv=*/false);
      for (const vista::PointCloudChunk& chunk : inst.point_chunks) {
        if (chunk.indices.empty()) {
          continue;
        }
        vista::TessMesh chunk_cpu;
        // half=0 made chunk markers invisible (zero-size cubes). Match the
        // non-chunked path so overlay clouds read on China DEM framing.
        if (!vista::tessellate_point_cloud_indexed(
                inst.point_positions.data(), chunk.indices.data(),
                chunk.indices.size(), 0.07f, chunk_cpu)) {
          continue;
        }
        GpuMesh chunk_mesh;
        chunk_mesh.kind = inst.kind;
        chunk_mesh.vertex = nullptr;
        chunk_mesh.index = nullptr;
        chunk_mesh.texture = nullptr;
        chunk_mesh.index_count = 0;
        chunk_mesh.stride = detail::kPositionStride;
        chunk_mesh.solid_r = mesh.solid_r;
        chunk_mesh.solid_g = mesh.solid_g;
        chunk_mesh.solid_b = mesh.solid_b;
        chunk_mesh.solid_a = mesh.solid_a;
        // Chunk path skipped the average-RGBA tint below — apply it here.
        if (!inst.point_rgba.empty() && inst.point_rgba.size() >= 4) {
          double sr = 0.0;
          double sg = 0.0;
          double sb = 0.0;
          const size_t pn = inst.point_rgba.size() / 4;
          for (size_t pi = 0; pi < pn; ++pi) {
            sr += inst.point_rgba[pi * 4];
            sg += inst.point_rgba[pi * 4 + 1];
            sb += inst.point_rgba[pi * 4 + 2];
          }
          chunk_mesh.solid_r = static_cast<float>(sr / (pn * 255.0));
          chunk_mesh.solid_g = static_cast<float>(sg / (pn * 255.0));
          chunk_mesh.solid_b = static_cast<float>(sb / (pn * 255.0));
          chunk_mesh.solid_a = 1.f;
        } else {
          // Do not inherit DEM land tint / legacy cyan default (0,1,1).
          chunk_mesh.solid_r = 220.f / 255.f;
          chunk_mesh.solid_g = 90.f / 255.f;
          chunk_mesh.solid_b = 40.f / 255.f;
          chunk_mesh.solid_a = 1.f;
        }
        chunk_mesh.line_width = 1.f;
        chunk_mesh.circle_radius = 5.f;
        if (inst.has_paint) {
          detail::apply_paint_scalars(inst.paint, &chunk_mesh);
        }
        if (!detail::upload_mesh(device, chunk_cpu.positions.data(),
                         chunk_cpu.positions.size(), chunk_cpu.indices.data(),
                         chunk_cpu.indices.size(), false, with_normals, false,
                         nullptr, &chunk_mesh)) {
          clear_meshes();
          for (GpuMesh& leftover : prev_meshes) {
            device->destroy_buffer(leftover.vertex);
            device->destroy_buffer(leftover.index);
            device->destroy_texture(leftover.texture);
          }
          return false;
        }
        chunk_mesh.aabb_min_x = static_cast<float>(chunk.min_x);
        chunk_mesh.aabb_min_y = static_cast<float>(chunk.min_y);
        chunk_mesh.aabb_min_z = static_cast<float>(chunk.min_z);
        chunk_mesh.aabb_max_x = static_cast<float>(chunk.max_x);
        chunk_mesh.aabb_max_y = static_cast<float>(chunk.max_y);
        chunk_mesh.aabb_max_z = static_cast<float>(chunk.max_z);
        meshes_.push_back(chunk_mesh);
      }
      continue;
    }

    vista::TessMesh cpu;
    bool have = false;
    if (inst.kind == vista::NodeKind::kVectorLayer &&
        (inst.ogr_layer || !inst.geoms.empty())) {
      have = detail::tessellate_vector_instance(inst, world_units_per_pixel, cpu);
    } else if (inst.kind == vista::NodeKind::kVectorLayer && inst.tin) {
      have = vista::tessellate_tin(inst.tin, cpu);
    } else if (inst.kind == vista::NodeKind::kVectorLayer && inst.grid) {
      have = vista::tessellate_grid(inst.grid, inst.grid_nx, inst.grid_ny, cpu);
    } else if (inst.kind == vista::NodeKind::kRasterLayer) {
      have = vista::tessellate_aabb(inst.min_x, inst.min_y, inst.min_z,
                                    inst.max_x, inst.max_y, inst.max_z, cpu);
    } else if (inst.kind == vista::NodeKind::kModel && inst.model) {
      vista::Mesh flat;
      if (vista::flatten_meshes(*inst.model, flat)) {
        cpu.positions = flat.positions;
        cpu.indices = flat.indices;
        have = true;
      }
    } else if (inst.kind == vista::NodeKind::kTerrain) {
      // Prefer CPU DEM mesh from World; else geom_3d; else AABB box.
      if (!inst.terrain_positions.empty() && !inst.terrain_indices.empty() &&
          (inst.terrain_positions.size() % 3) == 0 &&
          (inst.terrain_indices.size() % 3) == 0) {
        cpu.positions = inst.terrain_positions;
        cpu.indices = inst.terrain_indices;
        have = true;
      } else if (inst.geom_3d) {
        have = vista::tessellate_3d_geometry(inst.geom_3d, cpu);
      } else {
        have = vista::tessellate_aabb(inst.min_x, inst.min_y, inst.min_z,
                                    inst.max_x, inst.max_y, inst.max_z, cpu);
      }
    } else if (inst.kind == vista::NodeKind::kPointCloud) {
      if (!inst.point_positions.empty() &&
          (inst.point_positions.size() % 3) == 0) {
        // Orbit-normalized China DEM span is ~3.2; half must read as markers
        // over terrain (0.025 vanished on full-country framing).
        have = vista::tessellate_point_cloud(inst.point_positions.data(),
                                           inst.point_positions.size() / 3,
                                           0.07f, cpu);
      } else if (inst.geom_3d) {
        have = vista::tessellate_3d_geometry(inst.geom_3d, cpu);
      } else {
        have = vista::tessellate_aabb(inst.min_x, inst.min_y, inst.min_z,
                                    inst.max_x, inst.max_y, inst.max_z, cpu);
      }
    } else if (inst.kind == vista::NodeKind::kModel && inst.geom_3d) {
      have = vista::tessellate_3d_geometry(inst.geom_3d, cpu);
    } else if (inst.kind == vista::NodeKind::kTileset) {
      // Prefer TilesetContentCache (present/orbit ensure_tileset_content), then
      // decode_content_file. Failed / missing URIs keep the AABB bridge.
      for (const std::string& uri : inst.visible_uris) {
        if (uri.empty()) {
          continue;
        }
        const vista::ModelAsset* cached_asset = nullptr;
        if (tileset_content_cache_) {
          const vista::TilesetContentEntry* entry =
              tileset_content_cache_->try_get(uri);
          if (entry) {
            if (!entry->decode_ok) {
              continue;
            }
            cached_asset = &entry->asset;
          }
        }
        vista::ModelAsset decoded;
        const vista::ModelAsset* asset = cached_asset;
        if (!asset) {
          if (!vista::decode_content_file(uri.c_str(), decoded)) {
            continue;
          }
          asset = &decoded;
        }
        vista::Mesh flat;
        if (!vista::flatten_meshes(*asset, flat) || flat.indices.empty()) {
          continue;
        }
        const uint32_t base =
            static_cast<uint32_t>(cpu.positions.size() / 3);
        cpu.positions.insert(cpu.positions.end(), flat.positions.begin(),
                             flat.positions.end());
        for (uint32_t idx : flat.indices) {
          cpu.indices.push_back(base + idx);
        }
        have = true;
      }
      if (!have) {
        have = vista::tessellate_aabb(inst.min_x, inst.min_y, inst.min_z,
                                    inst.max_x, inst.max_y, inst.max_z, cpu);
      }
    }
    if (!have) {
      continue;
    }
    const bool terrain_tex =
        inst.kind == vista::NodeKind::kTerrain && !inst.terrain_rgba.empty() &&
        inst.terrain_tex_w > 0 && inst.terrain_tex_h > 0 &&
        inst.terrain_rgba.size() >=
            static_cast<size_t>(inst.terrain_tex_w) *
                static_cast<size_t>(inst.terrain_tex_h) * 4u;
    render::rhi::Texture* terrain_gpu_tex = nullptr;
    if (terrain_tex) {
      terrain_gpu_tex = detail::upload_rgba_texture_wh(device, inst.terrain_rgba.data(),
                                               inst.terrain_tex_w,
                                               inst.terrain_tex_h);
    }
    const bool terrain_tex_ok = terrain_gpu_tex != nullptr;
    if (terrain_tex_ok) {
      cpu.has_image = true;
    }
    const bool with_uv = terrain_tex_ok || want_symbol;
    // Lit kinds upload POSITION+NORMAL unless textured (UV path wins).
    const bool with_normals = detail::upload_with_normals(inst.kind, with_uv);
    const bool uv_on_xz = terrain_tex_ok;
    const float* explicit_uvs = nullptr;
    if (terrain_tex_ok &&
        inst.terrain_uvs.size() == (cpu.positions.size() / 3) * 2) {
      explicit_uvs = inst.terrain_uvs.data();
    }
    if (!detail::upload_mesh(device, cpu.positions.data(), cpu.positions.size(),
                     cpu.indices.data(), cpu.indices.size(), with_uv,
                     with_normals, uv_on_xz, explicit_uvs, &mesh)) {
      if (terrain_gpu_tex) {
        device->destroy_texture(terrain_gpu_tex);
      }
      clear_meshes();
      for (GpuMesh& leftover : prev_meshes) {
        device->destroy_buffer(leftover.vertex);
        device->destroy_buffer(leftover.index);
        device->destroy_texture(leftover.texture);
      }
      return false;
    }
    // Frustum cull must use vertex/world draw space. Instance min/max may still
    // be geographic lon/lat while DEM vertices are orbit-normalized.
    if (cpu.positions.size() >= 3) {
      float mn_x = cpu.positions[0];
      float mn_y = cpu.positions[1];
      float mn_z = cpu.positions[2];
      float mx_x = mn_x;
      float mx_y = mn_y;
      float mx_z = mn_z;
      for (size_t i = 0; i + 2 < cpu.positions.size(); i += 3) {
        const float x = cpu.positions[i];
        const float y = cpu.positions[i + 1];
        const float z = cpu.positions[i + 2];
        if (x < mn_x) {
          mn_x = x;
        }
        if (y < mn_y) {
          mn_y = y;
        }
        if (z < mn_z) {
          mn_z = z;
        }
        if (x > mx_x) {
          mx_x = x;
        }
        if (y > mx_y) {
          mx_y = y;
        }
        if (z > mx_z) {
          mx_z = z;
        }
      }
      mesh.aabb_min_x = mn_x;
      mesh.aabb_min_y = mn_y;
      mesh.aabb_min_z = mn_z;
      mesh.aabb_max_x = mx_x;
      mesh.aabb_max_y = mx_y;
      mesh.aabb_max_z = mx_z;
    }
    if (terrain_tex_ok) {
      mesh.texture = terrain_gpu_tex;
      // Boost land readability: hypsometric * warm land tint (not pure white).
      // Keep Style paint albedo when set_instance_paint already filled solid_*.
      if (!inst.has_paint) {
        if (solid_terrain_forced_) {
          // Sky-on FlyCube textured samples read black. Prefer the gated tint
          // from update_solid_terrain (green-biased land mean) — recomputing
          // raw bake mean here reintroduced brown rock and zeroed
          // green_land_frac on plugin.world3d.
          mesh.solid_r = solid_r_;
          mesh.solid_g = solid_g_;
          mesh.solid_b = solid_b_;
          mesh.solid_a = 1.f;
        } else {
          // Identity tint: hypsometric bake already carries land colors.
          mesh.solid_r = 1.f;
          mesh.solid_g = 1.f;
          mesh.solid_b = 1.f;
          mesh.solid_a = 1.f;
        }
      }
    } else if (inst.kind == vista::NodeKind::kPointCloud) {
      if (!inst.point_rgba.empty() && inst.point_rgba.size() >= 4) {
        // Average RGB so hypsometric / LAS color reads without vertex colors.
        double sr = 0.0;
        double sg = 0.0;
        double sb = 0.0;
        const size_t pn = inst.point_rgba.size() / 4;
        for (size_t pi = 0; pi < pn; ++pi) {
          sr += inst.point_rgba[pi * 4];
          sg += inst.point_rgba[pi * 4 + 1];
          sb += inst.point_rgba[pi * 4 + 2];
        }
        mesh.solid_r = static_cast<float>(sr / (pn * 255.0));
        mesh.solid_g = static_cast<float>(sg / (pn * 255.0));
        mesh.solid_b = static_cast<float>(sb / (pn * 255.0));
        mesh.solid_a = 1.f;
      } else {
        mesh.solid_r = 220.f / 255.f;
        mesh.solid_g = 90.f / 255.f;
        mesh.solid_b = 40.f / 255.f;
        mesh.solid_a = 1.f;
      }
    } else if (inst.kind == vista::NodeKind::kTerrain && !inst.has_paint) {
      mesh.solid_r = 0.22f;
      mesh.solid_g = 0.58f;
      mesh.solid_b = 0.20f;
      mesh.solid_a = 1.f;
    } else if (cpu.has_image && inst.layer) {
      mesh.texture = detail::upload_layer_texture(device, inst.layer);
    } else if (want_symbol) {
      mesh.texture = detail::upload_symbol_texture(device, inst.paint.symbol);
    }
    meshes_.push_back(mesh);
  }
  for (GpuMesh& leftover : prev_meshes) {
    device->destroy_buffer(leftover.vertex);
    device->destroy_buffer(leftover.index);
    device->destroy_texture(leftover.texture);
  }
  upload_width_ = width;
  upload_height_ = height;
  meshes_dirty_ = false;
  return true;
}

}  // namespace vista
