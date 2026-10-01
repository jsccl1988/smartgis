// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/scene/detail/upload.h"

#include "effect/scene/detail/tessellate.h"

#include <cstdlib>
#include <fstream>
#include <vector>

namespace effect {
namespace scene {
namespace detail {

bool is_lit_kind(gis::NodeKind kind) {
  return kind == gis::NodeKind::kTerrain || kind == gis::NodeKind::kModel ||
         kind == gis::NodeKind::kTileset || kind == gis::NodeKind::kPointCloud;
}

bool want_lit_terrain() {
  if (const char* e = std::getenv("SMT_SCENE3D_LIT_TERRAIN")) {
    return e[0] == '1' && e[1] == '\0';
  }
  return false;
}

bool upload_with_normals(gis::NodeKind kind, bool with_uv) {
  if (!is_lit_kind(kind) || with_uv) {
    return false;
  }
  // Terrain / point-cloud default to unlit solid (position-only stride).
  if ((kind == gis::NodeKind::kTerrain || kind == gis::NodeKind::kPointCloud) &&
      !want_lit_terrain()) {
    return false;
  }
  return true;
}

render::rhi::TextureDesc texture_desc_from_bytes(uint32_t byte_size) {
  render::rhi::TextureDesc desc;
  desc.format = render::rhi::TextureFormat::kRgba8;
  if (byte_size >= 4 && (byte_size % 4) == 0) {
    const uint32_t pixels = byte_size / 4;
    uint32_t side = 1;
    while ((side + 1) * (side + 1) <= pixels) {
      ++side;
    }
    if (side * side == pixels) {
      desc.width = side;
      desc.height = side;
    } else {
      desc.width = pixels;
      desc.height = 1;
    }
  } else {
    desc.width = (byte_size + 3) / 4;
    if (desc.width == 0) {
      desc.width = 1;
    }
    desc.height = 1;
  }
  return desc;
}

bool layer_image_pixels(const gis::SmtLayer* layer, const void** data,
                        uint32_t* byte_size) {
  if (!layer || !data || !byte_size) {
    return false;
  }
  if (layer->GetLayerType() == gis::LYR_TITLE) {
    const auto* tiles = static_cast<const gis::SmtTileLayer*>(layer);
    const int n = tiles->GetTileCount();
    for (int i = 0; i < n; ++i) {
      const base::SmtTile* tile = tiles->GetTile(i);
      if (tile && tile->pTileBuf && tile->lTileBufSize > 0) {
        *data = tile->pTileBuf;
        *byte_size = static_cast<uint32_t>(tile->lTileBufSize);
        return true;
      }
    }
    return false;
  }
  if (layer->GetLayerType() != gis::LYR_RASTER) {
    return false;
  }
  const auto* raster = static_cast<const gis::SmtRasterLayer*>(layer);
  char* buf = nullptr;
  long size = 0;
  long code = 0;
  base::fRect loc;
  if (raster->GetRasterNoClone(buf, size, loc, code) == SMT_ERR_NONE && buf &&
      size > 0) {
    *data = buf;
    *byte_size = static_cast<uint32_t>(size);
    return true;
  }
  return false;
}

render::rhi::Texture* upload_rgba_texture(render::rhi::Device* device,
                                          const void* pixels,
                                          uint32_t byte_size) {
  if (!device || !pixels || byte_size == 0) {
    return nullptr;
  }
  render::rhi::TextureDesc desc = texture_desc_from_bytes(byte_size);
  render::rhi::Texture* texture = device->create_texture(desc);
  if (!texture) {
    return nullptr;
  }
  const uint32_t upload_bytes =
      byte_size < texture->byte_size() ? byte_size : texture->byte_size();
  if (!device->upload_texture(texture, pixels, upload_bytes)) {
    device->destroy_texture(texture);
    return nullptr;
  }
  return texture;
}

render::rhi::Texture* upload_rgba_texture_wh(render::rhi::Device* device,
                                             const void* pixels, uint32_t width,
                                             uint32_t height) {
  if (!device || !pixels || width == 0 || height == 0) {
    return nullptr;
  }
  render::rhi::TextureDesc desc;
  desc.width = width;
  desc.height = height;
  desc.format = render::rhi::TextureFormat::kRgba8;
  desc.usage =
      render::rhi::TextureUsage::kSampled | render::rhi::TextureUsage::kCopyDest;
  render::rhi::Texture* texture = device->create_texture(desc);
  if (!texture) {
    return nullptr;
  }
  const uint32_t byte_size = width * height * 4u;
  if (!device->upload_texture(texture, pixels, byte_size)) {
    device->destroy_texture(texture);
    return nullptr;
  }
  return texture;
}

render::rhi::Texture* upload_layer_texture(render::rhi::Device* device,
                                           const gis::SmtLayer* layer) {
  const void* pixels = nullptr;
  uint32_t byte_size = 0;
  if (!layer_image_pixels(layer, &pixels, &byte_size)) {
    return nullptr;
  }
  return upload_rgba_texture(device, pixels, byte_size);
}

// Upload symbol icon pixels. Prefers in-memory bytes; if only a path is set,
// tries a binary file read. Encoded image formats (PNG/JPEG) are not decoded
// here — path must already hold raw RGBA8 (or the read is skipped).
render::rhi::Texture* upload_symbol_texture(
    render::rhi::Device* device, const gis::style::SymbolEntry& symbol) {
  if (!device) {
    return nullptr;
  }
  if (!symbol.bytes.empty()) {
    return upload_rgba_texture(device, symbol.bytes.data(),
                               static_cast<uint32_t>(symbol.bytes.size()));
  }
  if (symbol.path.empty()) {
    return nullptr;
  }
  std::ifstream in(symbol.path, std::ios::binary | std::ios::ate);
  if (!in) {
    // Path present but unreadable (missing file or no permission): skip icon.
    return nullptr;
  }
  const std::streamoff end = in.tellg();
  if (end <= 0) {
    return nullptr;
  }
  in.seekg(0, std::ios::beg);
  std::vector<uint8_t> bytes(static_cast<size_t>(end));
  if (!in.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()))) {
    return nullptr;
  }
  // Only accept sizes that look like raw RGBA8; skip compressed image files.
  if (bytes.size() < 4 || (bytes.size() % 4) != 0) {
    return nullptr;
  }
  return upload_rgba_texture(device, bytes.data(),
                             static_cast<uint32_t>(bytes.size()));
}

bool upload_mesh(render::rhi::Device* device, const float* positions,
                 size_t position_count, const uint32_t* indices,
                 size_t index_count, bool with_uv, bool with_normals,
                 bool uv_on_xz, const float* explicit_uvs,
                 GpuScene::GpuMesh* out) {
  if (!device || !out || !positions || !indices || position_count == 0 ||
      index_count == 0 || (position_count % 3) != 0) {
    return false;
  }
  std::vector<float> interleaved;
  const float* vb_data = positions;
  uint32_t vb_bytes = static_cast<uint32_t>(position_count * sizeof(float));
  uint32_t stride = kPositionStride;
  if (with_uv) {
    const size_t verts = position_count / 3;
    interleaved.resize(verts * 5);
    const bool have_explicit =
        explicit_uvs != nullptr;
    if (have_explicit) {
      for (size_t i = 0; i < verts; ++i) {
        interleaved[i * 5 + 0] = positions[i * 3 + 0];
        interleaved[i * 5 + 1] = positions[i * 3 + 1];
        interleaved[i * 5 + 2] = positions[i * 3 + 2];
        interleaved[i * 5 + 3] = explicit_uvs[i * 2 + 0];
        interleaved[i * 5 + 4] = explicit_uvs[i * 2 + 1];
      }
    } else {
      // Terrain drape uses X/Z (lon/lat plane); 2D rasters use X/Y.
      const int u_axis = 0;
      const int v_axis = uv_on_xz ? 2 : 1;
      float minu = positions[u_axis];
      float minv = positions[v_axis];
      float maxu = minu;
      float maxv = minv;
      for (size_t i = 0; i < verts; ++i) {
        const float u = positions[i * 3 + static_cast<size_t>(u_axis)];
        const float v = positions[i * 3 + static_cast<size_t>(v_axis)];
        if (u < minu) {
          minu = u;
        }
        if (v < minv) {
          minv = v;
        }
        if (u > maxu) {
          maxu = u;
        }
        if (v > maxv) {
          maxv = v;
        }
      }
      const float du = (maxu - minu) > 1e-6f ? (maxu - minu) : 1.f;
      const float dv = (maxv - minv) > 1e-6f ? (maxv - minv) : 1.f;
      for (size_t i = 0; i < verts; ++i) {
        interleaved[i * 5 + 0] = positions[i * 3 + 0];
        interleaved[i * 5 + 1] = positions[i * 3 + 1];
        interleaved[i * 5 + 2] = positions[i * 3 + 2];
        interleaved[i * 5 + 3] =
            (positions[i * 3 + static_cast<size_t>(u_axis)] - minu) / du;
        // DEM bake / imagery: row0 = north. Orbit Z = lat; north is max Z.
        // Flip V so AABB fallback matches explicit mesh-grid UVs.
        const float vn =
            (positions[i * 3 + static_cast<size_t>(v_axis)] - minv) / dv;
        interleaved[i * 5 + 4] = uv_on_xz ? (1.f - vn) : vn;
      }
    }
    vb_data = interleaved.data();
    vb_bytes = static_cast<uint32_t>(interleaved.size() * sizeof(float));
    stride = 5 * sizeof(float);
  } else if (with_normals) {
    // Lit solid: POSITION + NORMAL (matches FlyCube lit VS input layout).
    interleave_positions_with_normals(positions, position_count, indices,
                                      index_count, &interleaved);
    if (interleaved.empty()) {
      return false;
    }
    vb_data = interleaved.data();
    vb_bytes = static_cast<uint32_t>(interleaved.size() * sizeof(float));
    stride = kLitPositionNormalStride;
  }
  const uint32_t ib_bytes = static_cast<uint32_t>(index_count * sizeof(uint32_t));
  out->vertex = device->create_buffer(vb_bytes, render::rhi::BufferUsage::kVertex);
  out->index = device->create_buffer(ib_bytes, render::rhi::BufferUsage::kIndex);
  if (!out->vertex || !out->index) {
    device->destroy_buffer(out->vertex);
    device->destroy_buffer(out->index);
    out->vertex = nullptr;
    out->index = nullptr;
    return false;
  }
  if (!device->upload(out->vertex, vb_data, vb_bytes) ||
      !device->upload(out->index, indices, ib_bytes)) {
    device->destroy_buffer(out->vertex);
    device->destroy_buffer(out->index);
    out->vertex = nullptr;
    out->index = nullptr;
    return false;
  }
  out->index_count = static_cast<uint32_t>(index_count);
  out->stride = stride;
  return true;
}

}  // namespace detail
}  // namespace scene
}  // namespace effect
