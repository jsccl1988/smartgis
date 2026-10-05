// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/map/detail/upload.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include "vista/component/map/multiply.h"
#include "render/rhi/rhi.h"

namespace vista {
namespace detail {
namespace {

constexpr uint32_t kPositionStride = 3u * sizeof(float);
constexpr uint32_t kUvStride = 5u * sizeof(float);
constexpr uint32_t kXyzFloats = 3u;
constexpr uint32_t kXyzuvFloats = 5u;

// CPU-side mega buffer for one merge key (solid XYZ, or UV + one texture).
struct MeshBucket {
  std::vector<float> verts;
  std::vector<uint32_t> indices;
  uint32_t floats_per_vert = kXyzFloats;
  size_t packed_floats = 0;
  size_t packed_indices = 0;
  render::rhi::Buffer* vb = nullptr;
  render::rhi::Buffer* ib = nullptr;
};

// Pending draw before device create_buffer; bucket_index selects MeshBucket.
struct PendingDraw {
  bool textured = false;
  size_t bucket_index = 0;
  render::rhi::Texture* texture = nullptr;
  uint32_t first_index = 0;
  uint32_t index_count = 0;
  float r = 0.f;
  float g = 0.f;
  float b = 0.f;
  float a = 1.f;
  render::rhi::BlendMode blend = render::rhi::BlendMode::kSrcAlpha;
};

bool create_mega_buffers(render::rhi::Device* device, MeshBucket* bucket,
                         std::vector<render::rhi::Buffer*>* keep) {
  if (!device || !bucket || !keep || bucket->verts.empty() ||
      bucket->indices.empty()) {
    return false;
  }
  const uint32_t vb_bytes =
      static_cast<uint32_t>(bucket->verts.size() * sizeof(float));
  const uint32_t ib_bytes =
      static_cast<uint32_t>(bucket->indices.size() * sizeof(uint32_t));
  render::rhi::Buffer* vertex =
      device->create_buffer(vb_bytes, render::rhi::BufferUsage::kVertex);
  render::rhi::Buffer* index =
      device->create_buffer(ib_bytes, render::rhi::BufferUsage::kIndex);
  if (!vertex || !index ||
      !device->upload(vertex, bucket->verts.data(), vb_bytes) ||
      !device->upload(index, bucket->indices.data(), ib_bytes)) {
    device->destroy_buffer(vertex);
    device->destroy_buffer(index);
    return false;
  }
  keep->push_back(vertex);
  keep->push_back(index);
  bucket->vb = vertex;
  bucket->ib = index;
  return true;
}

void append_indices(MeshBucket* bucket, const std::vector<uint32_t>& indices,
                    uint32_t base, uint32_t* first_index,
                    uint32_t* index_count) {
  *first_index = static_cast<uint32_t>(bucket->indices.size());
  *index_count = static_cast<uint32_t>(indices.size());
  const size_t i0 = bucket->indices.size();
  bucket->indices.resize(i0 + indices.size());
  uint32_t* idst = bucket->indices.data() + i0;
  if (base == 0) {
    std::memcpy(idst, indices.data(), indices.size() * sizeof(uint32_t));
    return;
  }
  for (uint32_t idx : indices) {
    *idst++ = base + idx;
  }
}

void append_xyz(MeshBucket* bucket, const std::vector<vista::Vertex>& verts,
                const std::vector<uint32_t>& indices, uint32_t* first_index,
                uint32_t* index_count) {
  const uint32_t base =
      static_cast<uint32_t>(bucket->verts.size() / kXyzFloats);
  const size_t v0 = bucket->verts.size();
  bucket->verts.resize(v0 + verts.size() * kXyzFloats);
  float* dst = bucket->verts.data() + v0;
  for (const vista::Vertex& v : verts) {
    // Vertex is xyzuv; solid mega keeps position only.
    std::memcpy(dst, &v.x, kXyzFloats * sizeof(float));
    dst += kXyzFloats;
  }
  append_indices(bucket, indices, base, first_index, index_count);
}

void append_xyzuv(MeshBucket* bucket,
                  const std::vector<vista::Vertex>& verts,
                  const std::vector<uint32_t>& indices, uint32_t* first_index,
                  uint32_t* index_count) {
  static_assert(sizeof(vista::Vertex) == kXyzuvFloats * sizeof(float),
                "Vertex must pack as xyzuv floats for mega UV memcpy");
  const uint32_t base =
      static_cast<uint32_t>(bucket->verts.size() / kXyzuvFloats);
  const size_t v0 = bucket->verts.size();
  bucket->verts.resize(v0 + verts.size() * kXyzuvFloats);
  float* dst = bucket->verts.data() + v0;
  std::memcpy(dst, verts.data(), verts.size() * sizeof(vista::Vertex));
  append_indices(bucket, indices, base, first_index, index_count);
}

// Merge consecutive meshes that share pipeline, texture, and tint into one
// draw range (same mega IB run). Non-adjacent same-key meshes stay separate
// so painter order with interleaved keys is preserved.
void push_or_coalesce(std::vector<PendingDraw>* pending, PendingDraw draw) {
  if (!pending->empty()) {
    PendingDraw& last = pending->back();
    if (last.textured == draw.textured && last.texture == draw.texture &&
        last.bucket_index == draw.bucket_index && last.blend == draw.blend &&
        last.r == draw.r && last.g == draw.g && last.b == draw.b &&
        last.a == draw.a &&
        last.first_index + last.index_count == draw.first_index) {
      last.index_count += draw.index_count;
      return;
    }
  }
  pending->push_back(draw);
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
                       uint32_t first_index, uint32_t stride,
                       render::rhi::Texture* texture, float r, float g, float b,
                       float a, render::rhi::BlendMode blend) {
  UploadedDraw draw;
  draw.textured = textured;
  draw.vertices = vb;
  draw.indices = ib;
  draw.index_count = index_count;
  draw.first_index = first_index;
  draw.stride = stride;
  draw.texture = texture;
  draw.r = r;
  draw.g = g;
  draw.b = b;
  draw.a = a;
  draw.blend = blend;
  return draw;
}

float clamp01(float v) {
  if (v < 0.f) {
    return 0.f;
  }
  if (v > 1.f) {
    return 1.f;
  }
  return v;
}

uint32_t float_bits(float v) {
  uint32_t bits = 0;
  static_assert(sizeof(float) == sizeof(uint32_t));
  std::memcpy(&bits, &v, sizeof(bits));
  return bits;
}

// kOver rasters share a texture by key. kMultiply bakes opacity into the
// texels, so the cache key includes that opacity and never aliases kOver.
struct RasterCacheKey {
  uint32_t key = 0;
  uint8_t multiply = 0;
  uint32_t opacity_bits = 0;
  bool operator==(const RasterCacheKey& other) const {
    return key == other.key && multiply == other.multiply &&
           opacity_bits == other.opacity_bits;
  }
};

struct RasterCacheKeyHash {
  size_t operator()(const RasterCacheKey& key) const noexcept {
    size_t h = static_cast<size_t>(key.key);
    h ^= static_cast<size_t>(key.multiply) + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= static_cast<size_t>(key.opacity_bits) + 0x9e3779b9u + (h << 6) +
         (h >> 2);
    return h;
  }
};

void bake_multiply_raster(std::vector<uint8_t>* rgba, int w, int h,
                          float opacity) {
  if (!rgba || w <= 0 || h <= 0) {
    return;
  }
  const size_t pixels = static_cast<size_t>(w) * static_cast<size_t>(h);
  if (rgba->size() < pixels * 4u) {
    return;
  }
  // Vista bakes luma coverage. |opacity| is unpacked mesh alpha
  // (packed A × DrawItem::opacity).
  vista::apply_multiply_coverage(
      std::span<std::uint8_t>(rgba->data(), pixels * 4u), clamp01(opacity));
}

}  // namespace

std::vector<UploadedDraw> upload_draws(
    render::rhi::Device* device, std::vector<PlacedMesh> meshes,
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

  // Pack from vertex_list()/index_list() while MapIR (or a sealed copy) is
  // alive. Do not seal here — that would deep-copy every borrow into
  // PlacedMesh owned vectors and then copy again into mega buffers (china
  // cold). FlyCube UpdateUploadBuffer reads the packed mega CPU spans during
  // create_mega_buffers below, not the DrawItem borrows.
  // Callers that destroy MapIR before upload must seal_borrowed_meshes first.

  render::rhi::Texture* atlas_texture = nullptr;
  if (atlas.width > 0 && atlas.height > 0) {
    const size_t need = static_cast<size_t>(atlas.width) *
                        static_cast<size_t>(atlas.height) * 4u;
    if (atlas.rgba.size() >= need) {
      atlas_texture = upload_rgba_texture(device, atlas.rgba.data(), atlas.width,
                                          atlas.height, textures);
    }
  }

  std::unordered_map<RasterCacheKey, render::rhi::Texture*, RasterCacheKeyHash>
      raster_cache;
  std::unordered_map<std::string, render::rhi::Texture*> icon_cache;

  auto cached_raster = [&](uint32_t key, bool multiply,
                           float opacity) -> render::rhi::Texture* {
    RasterCacheKey cache_key;
    cache_key.key = key;
    cache_key.multiply = multiply ? 1u : 0u;
    cache_key.opacity_bits = multiply ? float_bits(clamp01(opacity)) : 0u;
    const auto found = raster_cache.find(cache_key);
    if (found != raster_cache.end()) {
      return found->second;
    }
    render::rhi::Texture* texture = nullptr;
    if (load_raster) {
      std::vector<uint8_t> rgba;
      int w = 0;
      int h = 0;
      if (load_raster(key, &rgba, &w, &h)) {
        if (multiply) {
          bake_multiply_raster(&rgba, w, h, opacity);
        }
        texture = upload_rgba_checked(device, &rgba, w, h, textures);
      }
    }
    raster_cache.emplace(cache_key, texture);
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

  // Merge keys → mega buckets:
  //   solid XYZ          → one bucket (pipeline solid, no texture)
  //   glyph atlas UV     → one bucket (textured + atlas)
  //   raster key / icon  → one UV bucket per texture binding
  std::vector<MeshBucket> buckets;
  size_t solid_bucket = static_cast<size_t>(-1);
  size_t glyph_bucket = static_cast<size_t>(-1);
  std::unordered_map<render::rhi::Texture*, size_t> uv_buckets;

  auto ensure_solid = [&]() -> size_t {
    if (solid_bucket == static_cast<size_t>(-1)) {
      solid_bucket = buckets.size();
      MeshBucket bucket;
      bucket.floats_per_vert = kXyzFloats;
      buckets.push_back(std::move(bucket));
    }
    return solid_bucket;
  };

  auto ensure_uv = [&](render::rhi::Texture* texture,
                       size_t* dedicated) -> size_t {
    if (dedicated && *dedicated != static_cast<size_t>(-1)) {
      return *dedicated;
    }
    const auto found = uv_buckets.find(texture);
    if (found != uv_buckets.end()) {
      if (dedicated) {
        *dedicated = found->second;
      }
      return found->second;
    }
    const size_t index = buckets.size();
    MeshBucket bucket;
    bucket.floats_per_vert = kXyzuvFloats;
    buckets.push_back(std::move(bucket));
    uv_buckets.emplace(texture, index);
    if (dedicated) {
      *dedicated = index;
    }
    return index;
  };

  struct ResolvedMesh {
    bool skip = true;
    bool textured = false;
    bool uv = false;
    size_t bucket_index = 0;
    render::rhi::Texture* texture = nullptr;
    float r = 0.f;
    float g = 0.f;
    float b = 0.f;
    float a = 1.f;
    render::rhi::BlendMode blend = render::rhi::BlendMode::kSrcAlpha;
  };

  std::vector<ResolvedMesh> resolved(meshes.size());
  for (size_t i = 0; i < meshes.size(); ++i) {
    const PlacedMesh& mesh = meshes[i];
    if (mesh.glyph_group != 0 && atlas_texture == nullptr) {
      continue;
    }
    const std::vector<vista::Vertex>& verts = mesh.vertex_list();
    const std::vector<uint32_t>& indices = mesh.index_list();
    if (verts.empty() || indices.empty()) {
      continue;
    }

    ResolvedMesh item;
    item.skip = false;
    item.r = mesh.r;
    item.g = mesh.g;
    item.b = mesh.b;
    item.a = mesh.a;

    if (mesh.source == MeshSource::kSolid) {
      item.bucket_index = ensure_solid();
    } else if (mesh.source == MeshSource::kGlyph) {
      if (!atlas_texture) {
        continue;
      }
      item.textured = true;
      item.uv = true;
      item.texture = atlas_texture;
      item.bucket_index = ensure_uv(atlas_texture, &glyph_bucket);
      item.r = 1.f;
      item.g = 1.f;
      item.b = 1.f;
      item.a = 1.f;
    } else if (mesh.source == MeshSource::kRaster) {
      if (!load_raster) {
        continue;
      }
      const bool multiply = mesh.blend == vista::DrawBlend::kMultiply;
      const float shade_opacity = mesh.a;
      render::rhi::Texture* texture =
          cached_raster(mesh.raster_key, multiply, shade_opacity);
      if (!texture) {
        continue;
      }
      item.textured = true;
      item.uv = true;
      item.texture = texture;
      item.bucket_index = ensure_uv(texture, nullptr);
      if (multiply) {
        // Luma and opacity are in the texels. The shader tint must not fade
        // them again. RGB tint stays the unpacked draw color.
        item.a = 1.f;
        item.blend = render::rhi::BlendMode::kMultiply;
      }
    } else if (mesh.source == MeshSource::kIcon) {
      render::rhi::Texture* texture = nullptr;
      if (load_icon) {
        texture = cached_icon(mesh.symbol_id);
      }
      if (!texture) {
        if (!mesh.solid_if_icon_missing) {
          continue;
        }
        item.bucket_index = ensure_solid();
      } else {
        item.textured = true;
        item.uv = true;
        item.texture = texture;
        item.bucket_index = ensure_uv(texture, nullptr);
      }
    } else {
      continue;
    }

    MeshBucket& bucket = buckets[item.bucket_index];
    bucket.packed_floats += verts.size() * bucket.floats_per_vert;
    bucket.packed_indices += indices.size();
    resolved[i] = item;
  }

  // One reserve per mega-buffer. Per-mesh reserve(size+n) reallocated every
  // append (capacity==size) and copied the growing packing buffer O(n²).
  for (MeshBucket& bucket : buckets) {
    bucket.verts.reserve(bucket.packed_floats);
    bucket.indices.reserve(bucket.packed_indices);
  }

  std::vector<PendingDraw> pending;
  pending.reserve(meshes.size());
  for (size_t i = 0; i < meshes.size(); ++i) {
    const ResolvedMesh& item = resolved[i];
    if (item.skip) {
      continue;
    }
    const PlacedMesh& mesh = meshes[i];
    uint32_t first_index = 0;
    uint32_t index_count = 0;
    if (item.uv) {
      append_xyzuv(&buckets[item.bucket_index], mesh.vertex_list(),
                   mesh.index_list(), &first_index, &index_count);
    } else {
      append_xyz(&buckets[item.bucket_index], mesh.vertex_list(),
                 mesh.index_list(), &first_index, &index_count);
    }
    PendingDraw draw;
    draw.textured = item.textured;
    draw.bucket_index = item.bucket_index;
    draw.texture = item.texture;
    draw.first_index = first_index;
    draw.index_count = index_count;
    draw.r = item.r;
    draw.g = item.g;
    draw.b = item.b;
    draw.a = item.a;
    draw.blend = item.blend;
    push_or_coalesce(&pending, draw);
  }

  // Mega buffers hold the upload source. Drop place-owned feather copies and
  // borrow wrappers before create_buffer so peak RSS does not keep both.
  meshes.clear();
  meshes.shrink_to_fit();

  for (MeshBucket& bucket : buckets) {
    if (!create_mega_buffers(device, &bucket, buffers)) {
      // Leave draws empty for this failed bucket; skip pending that need it.
      bucket.vb = nullptr;
      bucket.ib = nullptr;
    }
    // Upload completed (or failed); free packed CPU before the next bucket.
    bucket.verts.clear();
    bucket.verts.shrink_to_fit();
    bucket.indices.clear();
    bucket.indices.shrink_to_fit();
  }

  draws.reserve(pending.size());
  for (const PendingDraw& p : pending) {
    if (p.bucket_index >= buckets.size()) {
      continue;
    }
    const MeshBucket& bucket = buckets[p.bucket_index];
    if (!bucket.vb || !bucket.ib) {
      continue;
    }
    const uint32_t stride =
        p.textured ? kUvStride : kPositionStride;
    draws.push_back(make_draw(p.textured, bucket.vb, bucket.ib, p.index_count,
                              p.first_index, stride, p.texture, p.r, p.g, p.b,
                              p.a, p.blend));
  }
  return draws;
}

}  // namespace detail
}  // namespace vista
