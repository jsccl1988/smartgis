// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/map/detail/atlas.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

#include "vista/component/map/color.h"
#include "vista/pass/map/pass.h"

namespace vista {
namespace detail {
namespace {

uint32_t quantize_size(float text_size_px) {
  if (text_size_px <= 0.f) {
    return 0;
  }
  return static_cast<uint32_t>(text_size_px * 64.f + 0.5f);
}

uint32_t quantize_opacity(float opacity) {
  return static_cast<uint32_t>(clampf(opacity, 0.f, 1.f) * 1000.f + 0.5f);
}

GlyphKey glyph_key(uint32_t codepoint, float text_size_px, uint32_t rgba,
                   float opacity) {
  GlyphKey key;
  key.codepoint = codepoint;
  key.size_q = quantize_size(text_size_px);
  key.rgba = rgba;
  key.opacity_q = quantize_opacity(opacity);
  return key;
}

GlyphKey glyph_key(const GlyphId& id) {
  return glyph_key(id.codepoint, id.text_size_px, id.rgba, id.opacity);
}

void blit_tinted(std::vector<uint8_t>* atlas, int atlas_w, int dx, int dy,
                 const GlyphRasterizer::Bitmap& bitmap, float r, float g,
                 float b, float opacity) {
  if (!atlas || bitmap.width <= 0 || bitmap.height <= 0) {
    return;
  }
  const size_t need = static_cast<size_t>(bitmap.width) *
                      static_cast<size_t>(bitmap.height) * 4u;
  if (bitmap.rgba.size() < need) {
    return;
  }
  for (int y = 0; y < bitmap.height; ++y) {
    for (int x = 0; x < bitmap.width; ++x) {
      const size_t si =
          (static_cast<size_t>(y) * static_cast<size_t>(bitmap.width) +
           static_cast<size_t>(x)) *
          4u;
      const float coverage = static_cast<float>(bitmap.rgba[si + 3]) / 255.f;
      const size_t di =
          (static_cast<size_t>(dy + y) * static_cast<size_t>(atlas_w) +
           static_cast<size_t>(dx + x)) *
          4u;
      if (di + 3 >= atlas->size()) {
        continue;
      }
      (*atlas)[di + 0] =
          static_cast<uint8_t>(clampf(r * coverage, 0.f, 1.f) * 255.f);
      (*atlas)[di + 1] =
          static_cast<uint8_t>(clampf(g * coverage, 0.f, 1.f) * 255.f);
      (*atlas)[di + 2] =
          static_cast<uint8_t>(clampf(b * coverage, 0.f, 1.f) * 255.f);
      (*atlas)[di + 3] = static_cast<uint8_t>(
          clampf(coverage * clampf(opacity, 0.f, 1.f), 0.f, 1.f) * 255.f);
    }
  }
}

void remap_cell_uv(const GlyphCell& cell, int atlas_w, int atlas_h,
                   std::vector<vista::Vertex>* verts) {
  if (!verts || atlas_w <= 0 || atlas_h <= 0) {
    return;
  }
  const float u0 = static_cast<float>(cell.x) / static_cast<float>(atlas_w);
  const float v0 = static_cast<float>(cell.y) / static_cast<float>(atlas_h);
  const float u1 =
      static_cast<float>(cell.x + cell.w) / static_cast<float>(atlas_w);
  const float v1 =
      static_cast<float>(cell.y + cell.h) / static_cast<float>(atlas_h);
  for (vista::Vertex& v : *verts) {
    v.u = u0 + v.u * (u1 - u0);
    v.v = v0 + v.v * (v1 - v0);
  }
}

}  // namespace

AtlasLayout pack_atlas(GlyphRasterizer* glyphs,
                       const vista::MapIR& frame) {
  AtlasLayout atlas;
  if (!glyphs) {
    return atlas;
  }
  struct Pending {
    GlyphKey key;
    GlyphRasterizer::Bitmap bitmap;
    float r = 1.f;
    float g = 1.f;
    float b = 1.f;
    float opacity = 1.f;
  };
  std::vector<Pending> pending;
  std::unordered_set<GlyphKey, GlyphKeyHash> seen;
  for (const vista::DrawItem& item : frame.items) {
    if (item.kind != vista::DrawKind::kText) {
      continue;
    }
    const GlyphKey key =
        glyph_key(item.codepoint, item.text_size_px, item.rgba, item.opacity);
    if (key.size_q == 0 || !seen.insert(key).second) {
      continue;
    }
    GlyphRasterizer::Bitmap bitmap;
    const float size_px = static_cast<float>(key.size_q) / 64.f;
    if (!glyphs->rasterize(item.codepoint, size_px, &bitmap) ||
        bitmap.width <= 0 || bitmap.height <= 0) {
      continue;
    }
    Pending row;
    row.key = key;
    unpack_rgba(item.rgba, item.opacity, &row.r, &row.g, &row.b, &row.opacity);
    row.bitmap = std::move(bitmap);
    pending.push_back(std::move(row));
  }
  if (pending.empty()) {
    return atlas;
  }

  constexpr int kPad = 1;
  int limit = 64;
  for (const Pending& row : pending) {
    limit = std::max(limit, row.bitmap.width + 2);
  }
  limit = std::min(limit, 2048);

  int cx = kPad;
  int cy = kPad;
  int row_h = 0;
  int used_w = kPad;
  int used_h = kPad;
  std::vector<GlyphCell> cells(pending.size());
  for (size_t i = 0; i < pending.size(); ++i) {
    const int w = pending[i].bitmap.width;
    const int h = pending[i].bitmap.height;
    if (cx + w + kPad > limit) {
      cy += row_h + kPad;
      cx = kPad;
      row_h = 0;
    }
    cells[i] = GlyphCell{cx, cy, w, h};
    cx += w + kPad;
    row_h = std::max(row_h, h);
    used_w = std::max(used_w, cx);
    used_h = std::max(used_h, cy + h + kPad);
  }
  if (used_w <= 0 || used_h <= 0 || used_h > 2048) {
    return atlas;
  }

  std::vector<uint8_t> pixels(static_cast<size_t>(used_w) *
                                  static_cast<size_t>(used_h) * 4u,
                              0);
  for (size_t i = 0; i < pending.size(); ++i) {
    blit_tinted(&pixels, used_w, cells[i].x, cells[i].y, pending[i].bitmap,
                pending[i].r, pending[i].g, pending[i].b, pending[i].opacity);
    atlas.cells.emplace(pending[i].key, cells[i]);
  }
  atlas.rgba = std::move(pixels);
  atlas.width = used_w;
  atlas.height = used_h;
  return atlas;
}

void bind_glyph_layout(std::vector<PlacedMesh>* meshes,
                       const AtlasLayout& layout) {
  if (!meshes) {
    return;
  }
  const bool layout_ok = layout.width > 0 && layout.height > 0;
  std::unordered_set<uint32_t> missing;
  for (const PlacedMesh& mesh : *meshes) {
    if (mesh.glyph_group == 0) {
      continue;
    }
    if (!layout_ok ||
        layout.cells.find(glyph_key(mesh.glyph)) == layout.cells.end()) {
      missing.insert(mesh.glyph_group);
    }
  }
  std::vector<PlacedMesh> kept;
  kept.reserve(meshes->size());
  for (PlacedMesh& mesh : *meshes) {
    if (mesh.glyph_group != 0 && missing.count(mesh.glyph_group) != 0) {
      continue;
    }
    if (mesh.source == MeshSource::kGlyph) {
      const auto found = layout.cells.find(glyph_key(mesh.glyph));
      if (found == layout.cells.end()) {
        continue;
      }
      remap_cell_uv(found->second, layout.width, layout.height, &mesh.vertices);
    }
    kept.push_back(std::move(mesh));
  }
  *meshes = std::move(kept);
}

}  // namespace detail
}  // namespace vista
