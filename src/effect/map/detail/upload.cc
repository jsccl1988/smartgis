// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/map/detail/upload.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "render/rhi/rhi.h"

namespace effect {
namespace map {
namespace detail {
namespace {

constexpr uint32_t kPositionStride = 3u * sizeof(float);
constexpr uint32_t kUvStride = 5u * sizeof(float);

bool upload_xyz(render::rhi::Device* device,
                const std::vector<gis::vista::Vertex>& verts,
                const std::vector<uint32_t>& indices,
                std::vector<render::rhi::Buffer*>* keep,
                render::rhi::Buffer** vb, render::rhi::Buffer** ib) {
  if (!device || !keep || !vb || !ib || verts.empty()) {
    return false;
  }
  std::vector<float> xyz(verts.size() * 3u);
  for (size_t i = 0; i < verts.size(); ++i) {
    xyz[i * 3u + 0] = verts[i].x;
    xyz[i * 3u + 1] = verts[i].y;
    xyz[i * 3u + 2] = verts[i].z;
  }
  const uint32_t vb_bytes = static_cast<uint32_t>(xyz.size() * sizeof(float));
  const uint32_t ib_bytes =
      static_cast<uint32_t>(indices.size() * sizeof(uint32_t));
  render::rhi::Buffer* vertex =
      device->create_buffer(vb_bytes, render::rhi::BufferUsage::kVertex);
  render::rhi::Buffer* index =
      device->create_buffer(ib_bytes, render::rhi::BufferUsage::kIndex);
  if (!vertex || !index || !device->upload(vertex, xyz.data(), vb_bytes) ||
      !device->upload(index, indices.data(), ib_bytes)) {
    device->destroy_buffer(vertex);
    device->destroy_buffer(index);
    return false;
  }
  keep->push_back(vertex);
  keep->push_back(index);
  *vb = vertex;
  *ib = index;
  return true;
}

bool upload_xyzuv(render::rhi::Device* device,
                  const std::vector<gis::vista::Vertex>& verts,
                  const std::vector<uint32_t>& indices,
                  std::vector<render::rhi::Buffer*>* keep,
                  render::rhi::Buffer** vb, render::rhi::Buffer** ib) {
  if (!device || !keep || !vb || !ib || verts.empty()) {
    return false;
  }
  std::vector<float> xyzuv(verts.size() * 5u);
  for (size_t i = 0; i < verts.size(); ++i) {
    xyzuv[i * 5u + 0] = verts[i].x;
    xyzuv[i * 5u + 1] = verts[i].y;
    xyzuv[i * 5u + 2] = verts[i].z;
    xyzuv[i * 5u + 3] = verts[i].u;
    xyzuv[i * 5u + 4] = verts[i].v;
  }
  const uint32_t vb_bytes =
      static_cast<uint32_t>(xyzuv.size() * sizeof(float));
  const uint32_t ib_bytes =
      static_cast<uint32_t>(indices.size() * sizeof(uint32_t));
  render::rhi::Buffer* vertex =
      device->create_buffer(vb_bytes, render::rhi::BufferUsage::kVertex);
  render::rhi::Buffer* index =
      device->create_buffer(ib_bytes, render::rhi::BufferUsage::kIndex);
  if (!vertex || !index || !device->upload(vertex, xyzuv.data(), vb_bytes) ||
      !device->upload(index, indices.data(), ib_bytes)) {
    device->destroy_buffer(vertex);
    device->destroy_buffer(index);
    return false;
  }
  keep->push_back(vertex);
  keep->push_back(index);
  *vb = vertex;
  *ib = index;
  return true;
}

render::rhi::Texture* upload_rgba_texture(
    render::rhi::Device* device, const uint8_t* rgba, int width, int height,
    std::vector<render::rhi::Texture*>* keep) {
  if (!device || !keep || !rgba || width <= 0 || height <= 0) {
    return nullptr;
  }
  const uint32_t bytes = static_cast<uint32_t>(width) *
                         static_cast<uint32_t>(height) * 4u;
  render::rhi::TextureDesc desc;
  desc.width = static_cast<uint32_t>(width);
  desc.height = static_cast<uint32_t>(height);
  desc.format = render::rhi::TextureFormat::kRgba8;
  desc.usage = render::rhi::TextureUsage::kSampled |
               render::rhi::TextureUsage::kCopyDest;
  render::rhi::Texture* texture = device->create_texture(desc);
  if (!texture || !device->upload_texture(texture, rgba, bytes)) {
    device->destroy_texture(texture);
    return nullptr;
  }
  keep->push_back(texture);
  return texture;
}

render::rhi::Texture* upload_rgba_checked(
    render::rhi::Device* device, const std::vector<uint8_t>* rgba, int w, int h,
    std::vector<render::rhi::Texture*>* keep) {
  if (!rgba || w <= 0 || h <= 0) {
    return nullptr;
  }
  const size_t need = static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;
  if (rgba->size() < need) {
    return nullptr;
  }
  return upload_rgba_texture(device, rgba->data(), w, h, keep);
}

UploadedDraw make_draw(bool textured, render::rhi::Buffer* vb,
                       render::rhi::Buffer* ib, uint32_t index_count,
                       uint32_t stride, render::rhi::Texture* texture, float r,
                       float g, float b, float a) {
  UploadedDraw draw;
  draw.textured = textured;
  draw.vertices = vb;
  draw.indices = ib;
  draw.index_count = index_count;
  draw.stride = stride;
  draw.texture = texture;
  draw.r = r;
  draw.g = g;
  draw.b = b;
  draw.a = a;
  return draw;
}

}  // namespace

std::vector<UploadedDraw> upload_draws(
    render::rhi::Device* device, const std::vector<PlacedMesh>& meshes,
    const AtlasLayout& atlas,
    const std::function<bool(uint32_t texture_key, std::vector<uint8_t>* rgba,
                             int* w, int* h)>& load_raster,
    const std::function<bool(const std::string& symbol_id,
                             std::vector<uint8_t>* rgba, int* w, int* h)>&
        load_icon,
    std::vector<render::rhi::Buffer*>* buffers,
    std::vector<render::rhi::Texture*>* textures) {
  std::vector<UploadedDraw> draws;
  if (!device || !buffers || !textures) {
    return draws;
  }

  render::rhi::Texture* atlas_texture = nullptr;
  if (atlas.width > 0 && atlas.height > 0) {
    const size_t need = static_cast<size_t>(atlas.width) *
                        static_cast<size_t>(atlas.height) * 4u;
    if (atlas.rgba.size() >= need) {
      atlas_texture = upload_rgba_texture(device, atlas.rgba.data(), atlas.width,
                                          atlas.height, textures);
    }
  }

  std::unordered_map<uint32_t, render::rhi::Texture*> raster_cache;
  std::unordered_map<std::string, render::rhi::Texture*> icon_cache;

  auto cached_raster = [&](uint32_t key) -> render::rhi::Texture* {
    const auto found = raster_cache.find(key);
    if (found != raster_cache.end()) {
      return found->second;
    }
    render::rhi::Texture* texture = nullptr;
    if (load_raster) {
      std::vector<uint8_t> rgba;
      int w = 0;
      int h = 0;
      if (load_raster(key, &rgba, &w, &h)) {
        texture = upload_rgba_checked(device, &rgba, w, h, textures);
      }
    }
    raster_cache.emplace(key, texture);
    return texture;
  };

  auto cached_icon = [&](const std::string& symbol_id) -> render::rhi::Texture* {
    const auto found = icon_cache.find(symbol_id);
    if (found != icon_cache.end()) {
      return found->second;
    }
    render::rhi::Texture* texture = nullptr;
    if (load_icon) {
      std::vector<uint8_t> rgba;
      int w = 0;
      int h = 0;
      if (load_icon(symbol_id, &rgba, &w, &h)) {
        texture = upload_rgba_checked(device, &rgba, w, h, textures);
      }
    }
    icon_cache.emplace(symbol_id, texture);
    return texture;
  };

  for (const PlacedMesh& mesh : meshes) {
    if (mesh.glyph_group != 0 && atlas_texture == nullptr) {
      continue;
    }
    const uint32_t index_count = static_cast<uint32_t>(mesh.indices.size());

    if (mesh.source == MeshSource::kSolid) {
      render::rhi::Buffer* vb = nullptr;
      render::rhi::Buffer* ib = nullptr;
      if (!upload_xyz(device, mesh.vertices, mesh.indices, buffers, &vb, &ib)) {
        continue;
      }
      draws.push_back(make_draw(false, vb, ib, index_count, kPositionStride,
                                nullptr, mesh.r, mesh.g, mesh.b, mesh.a));
      continue;
    }

    if (mesh.source == MeshSource::kGlyph) {
      if (!atlas_texture) {
        continue;
      }
      render::rhi::Buffer* vb = nullptr;
      render::rhi::Buffer* ib = nullptr;
      if (!upload_xyzuv(device, mesh.vertices, mesh.indices, buffers, &vb,
                        &ib)) {
        continue;
      }
      // Glyph tint and opacity are baked into the atlas; keep tint neutral.
      draws.push_back(make_draw(true, vb, ib, index_count, kUvStride,
                                atlas_texture, 1.f, 1.f, 1.f, 1.f));
      continue;
    }

    if (mesh.source == MeshSource::kRaster) {
      if (!load_raster) {
        continue;
      }
      render::rhi::Texture* texture = cached_raster(mesh.raster_key);
      if (!texture) {
        continue;
      }
      render::rhi::Buffer* vb = nullptr;
      render::rhi::Buffer* ib = nullptr;
      if (!upload_xyzuv(device, mesh.vertices, mesh.indices, buffers, &vb,
                        &ib)) {
        continue;
      }
      draws.push_back(make_draw(true, vb, ib, index_count, kUvStride, texture,
                                mesh.r, mesh.g, mesh.b, mesh.a));
      continue;
    }

    if (mesh.source == MeshSource::kIcon) {
      render::rhi::Texture* texture = nullptr;
      if (load_icon) {
        texture = cached_icon(mesh.symbol_id);
      }
      if (!texture) {
        if (!mesh.solid_if_icon_missing) {
          continue;
        }
        render::rhi::Buffer* vb = nullptr;
        render::rhi::Buffer* ib = nullptr;
        if (!upload_xyz(device, mesh.vertices, mesh.indices, buffers, &vb,
                        &ib)) {
          continue;
        }
        draws.push_back(make_draw(false, vb, ib, index_count, kPositionStride,
                                  nullptr, mesh.r, mesh.g, mesh.b, mesh.a));
        continue;
      }
      render::rhi::Buffer* vb = nullptr;
      render::rhi::Buffer* ib = nullptr;
      if (!upload_xyzuv(device, mesh.vertices, mesh.indices, buffers, &vb,
                        &ib)) {
        continue;
      }
      draws.push_back(make_draw(true, vb, ib, index_count, kUvStride, texture,
                                mesh.r, mesh.g, mesh.b, mesh.a));
    }
  }
  return draws;
}

}  // namespace detail
}  // namespace map
}  // namespace effect
