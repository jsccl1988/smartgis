// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/world/pass.h"

#include "base/core/log.h"
#include "vista/mesh/tessellate.h"
#include "vista/component/world/envelope.h"
#include "vista/component/world/kind_tess.h"
#include "vista/pass/world/detail/tint.h"
#include "vista/pass/world/detail/upload.h"

namespace vista {

void WorldPass::clear_meshes() {
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

void WorldPass::resolve_view_envelope(uint32_t width, uint32_t height,
                                     double* min_x, double* min_y,
                                     double* max_x, double* max_y) const {
  detail::ViewOrtho ortho;
  ortho.set = view_ortho_set_;
  ortho.min_x = view_min_x_;
  ortho.min_y = view_min_y_;
  ortho.max_x = view_max_x_;
  ortho.max_y = view_max_y_;
  detail::resolve_view_envelope(ortho, instances_, width, height, min_x, min_y,
                                max_x, max_y);
}

bool WorldPass::rebuild_meshes(render::rhi::Device* device, uint32_t width,
                             uint32_t height) {
  LOGGING(LOG_INFO, "WorldPass.rebuild_meshes size=%ux%u instances=%zu", width,
          height, instances_.size());
  if (!device || width == 0 || height == 0) {
    clear_meshes();
    return false;
  }
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
  for (const Instance& inst : instances_) {
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

    if (detail::uses_point_cloud_chunks(inst)) {
      const bool with_normals =
          detail::upload_with_normals(inst.kind, /*with_uv=*/false);
      for (const vista::PointCloudChunk& chunk : inst.point_chunks) {
        if (chunk.indices.empty()) {
          continue;
        }
        vista::TessMesh chunk_cpu;
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
        float pr = 0.f;
        float pg = 0.f;
        float pb = 0.f;
        if (detail::average_point_rgba(inst, &pr, &pg, &pb)) {
          chunk_mesh.solid_r = pr;
          chunk_mesh.solid_g = pg;
          chunk_mesh.solid_b = pb;
          chunk_mesh.solid_a = 1.f;
        } else {
          detail::apply_default_point_tint(&chunk_mesh);
        }
        chunk_mesh.line_width = 1.f;
        chunk_mesh.circle_radius = 5.f;
        if (inst.has_paint) {
          detail::apply_paint_scalars(inst.paint, &chunk_mesh);
        }
        if (!detail::upload_mesh(device, chunk_cpu.positions.data(),
                                 chunk_cpu.positions.size(),
                                 chunk_cpu.indices.data(),
                                 chunk_cpu.indices.size(), false, with_normals,
                                 false, nullptr, &chunk_mesh)) {
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
    if (!detail::tessellate_instance(inst, world_units_per_pixel,
                                     tileset_content_cache_, &cpu)) {
      continue;
    }
    if (inst.kind == vista::NodeKind::kTerrain) {
      if (!terrain_.prepare_mesh(device, inst, &cpu, &mesh, solid_r_, solid_g_,
                                 solid_b_, solid_a_)) {
        clear_meshes();
        for (GpuMesh& leftover : prev_meshes) {
          device->destroy_buffer(leftover.vertex);
          device->destroy_buffer(leftover.index);
          device->destroy_texture(leftover.texture);
        }
        return false;
      }
      meshes_.push_back(mesh);
      continue;
    }
    const bool with_uv = want_symbol;
    const bool with_normals = detail::upload_with_normals(inst.kind, with_uv);
    if (!detail::upload_mesh(device, cpu.positions.data(), cpu.positions.size(),
                             cpu.indices.data(), cpu.indices.size(), with_uv,
                             with_normals, /*uv_on_xz=*/false, nullptr,
                             &mesh)) {
      clear_meshes();
      for (GpuMesh& leftover : prev_meshes) {
        device->destroy_buffer(leftover.vertex);
        device->destroy_buffer(leftover.index);
        device->destroy_texture(leftover.texture);
      }
      return false;
    }
    if (cpu.positions.size() >= 3) {
      detail::aabb_from_xyz(cpu.positions.data(), cpu.positions.size(),
                            &mesh.aabb_min_x, &mesh.aabb_min_y, &mesh.aabb_min_z,
                            &mesh.aabb_max_x, &mesh.aabb_max_y,
                            &mesh.aabb_max_z);
    }
    if (inst.kind == vista::NodeKind::kPointCloud) {
      float pr = 0.f;
      float pg = 0.f;
      float pb = 0.f;
      if (detail::average_point_rgba(inst, &pr, &pg, &pb)) {
        mesh.solid_r = pr;
        mesh.solid_g = pg;
        mesh.solid_b = pb;
        mesh.solid_a = 1.f;
      } else {
        detail::apply_default_point_tint(&mesh);
      }
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
