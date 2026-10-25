// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/place/place.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#include "vista/component/map/layout/policy/slice_key.h"

#include "vista/component/map/color.h"

namespace vista {
namespace detail {
namespace {

bool indices_fit(const std::vector<uint32_t>& indices, size_t vertex_count) {
  if (indices.empty() || vertex_count == 0) {
    return false;
  }
  for (uint32_t index : indices) {
    if (static_cast<size_t>(index) >= vertex_count) {
      return false;
    }
  }
  return true;
}

// Viewport pixels are y-down. The lon/lat ortho is y-up, matching
// WorldPass::set_view_ortho → make_ortho_camera(min_x, max_x, min_y, max_y).
void map_pixel_to_ortho(const vista::View& view, float* x, float* y) {
  const float width = static_cast<float>(view.width_px);
  const float height = static_cast<float>(view.height_px);
  if (width == 0.f || height == 0.f || !x || !y) {
    return;
  }
  const float min_x = static_cast<float>(view.min_x);
  const float max_x = static_cast<float>(view.max_x);
  const float min_y = static_cast<float>(view.min_y);
  const float max_y = static_cast<float>(view.max_y);
  const float px = *x;
  const float py = *y;
  *x = min_x + (px / width) * (max_x - min_x);
  *y = max_y - (py / height) * (max_y - min_y);
}

// angle_rad is atan2(dy, dx) in pixel space (x right, y down), so +x rotates
// toward +y on the screen.
void rotate_about_anchor(std::vector<vista::Vertex>* verts, float anchor_x,
                         float anchor_y, float angle_rad) {
  if (!verts || angle_rad == 0.f) {
    return;
  }
  const float c = std::cos(angle_rad);
  const float s = std::sin(angle_rad);
  for (vista::Vertex& v : *verts) {
    const float dx = v.x - anchor_x;
    const float dy = v.y - anchor_y;
    v.x = anchor_x + dx * c - dy * s;
    v.y = anchor_y + dx * s + dy * c;
  }
}

void to_ortho(const vista::View& view, bool pixel_space,
              std::vector<vista::Vertex>* verts) {
  if (!pixel_space || !verts) {
    return;
  }
  for (vista::Vertex& v : *verts) {
    map_pixel_to_ortho(view, &v.x, &v.y);
  }
}

void expand_from_center(std::vector<vista::Vertex>* verts, float pad) {
  if (!verts || verts->empty() || pad <= 0.f) {
    return;
  }
  float cx = 0.f;
  float cy = 0.f;
  for (const vista::Vertex& v : *verts) {
    cx += v.x;
    cy += v.y;
  }
  const float inv = 1.f / static_cast<float>(verts->size());
  cx *= inv;
  cy *= inv;
  for (vista::Vertex& v : *verts) {
    const float dx = v.x - cx;
    const float dy = v.y - cy;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0e-4f) {
      continue;
    }
    v.x += (dx / len) * pad;
    v.y += (dy / len) * pad;
  }
}

bool uvs_unset(const std::vector<vista::Vertex>& verts) {
  for (const vista::Vertex& v : verts) {
    if (v.u != 0.f || v.v != 0.f) {
      return false;
    }
  }
  return true;
}

// World quads: v = 0 on the north edge (max y), matching raster tiles.
void assign_bbox_uv(std::vector<vista::Vertex>* verts) {
  if (!verts || verts->empty()) {
    return;
  }
  float min_x = (*verts)[0].x;
  float min_y = (*verts)[0].y;
  float max_x = min_x;
  float max_y = min_y;
  for (const vista::Vertex& v : *verts) {
    min_x = std::min(min_x, v.x);
    min_y = std::min(min_y, v.y);
    max_x = std::max(max_x, v.x);
    max_y = std::max(max_y, v.y);
  }
  const float du = std::max(max_x - min_x, 1.0e-6f);
  const float dv = std::max(max_y - min_y, 1.0e-6f);
  for (vista::Vertex& v : *verts) {
    v.u = (v.x - min_x) / du;
    v.v = (max_y - v.y) / dv;
  }
}

// Local 0..1 glyph UVs from pre-transform positions. The atlas binder maps
// these into a cell. Pixel y grows down (atlas row 0 at the top). World
// quads grow up, so V is flipped.
void write_local_glyph_uv(bool pixel_space,
                          std::vector<vista::Vertex>* verts) {
  if (!verts || verts->empty()) {
    return;
  }
  float min_x = (*verts)[0].x;
  float min_y = (*verts)[0].y;
  float max_x = min_x;
  float max_y = min_y;
  for (const vista::Vertex& v : *verts) {
    min_x = std::min(min_x, v.x);
    min_y = std::min(min_y, v.y);
    max_x = std::max(max_x, v.x);
    max_y = std::max(max_y, v.y);
  }
  const float du = std::max(max_x - min_x, 1.0e-6f);
  const float dv = std::max(max_y - min_y, 1.0e-6f);
  for (vista::Vertex& v : *verts) {
    const float su = (v.x - min_x) / du;
    float sv = (v.y - min_y) / dv;
    if (!pixel_space) {
      sv = 1.f - sv;
    }
    v.u = su;
    v.v = sv;
  }
}

void tag_text(PlacedMesh* mesh, const vista::DrawItem& item,
              uint32_t group) {
  mesh->glyph_group = group;
  mesh->glyph.codepoint = item.codepoint;
  mesh->glyph.text_size_px = item.text_size_px;
  mesh->glyph.rgba = item.rgba;
  mesh->glyph.opacity = item.opacity;
  mesh->glyph.font = static_cast<uint8_t>(item.font);
}

// One screen pixel in world units (same scale as line half-width from Layout).
float world_half_pixel(const vista::View& view) {
  if (view.width_px == 0 || view.height_px == 0) {
    return 0.f;
  }
  const float sx = static_cast<float>(std::fabs(view.max_x - view.min_x)) /
                   static_cast<float>(view.width_px);
  const float sy = static_cast<float>(std::fabs(view.max_y - view.min_y)) /
                   static_cast<float>(view.height_px);
  // 1.0 px pad (was 0.5) — China outline / rivers read less jagged on FlyCube.
  return 1.0f * (std::max)(sx, sy);
}

// Phase 2a: soft radial pad under the core solid for softer edges (no MSAA).
constexpr float kFeatherAlphaScale = 0.32f;

void push_feather_under(PlacedMesh core, float pad, std::vector<PlacedMesh>* out) {
  const std::vector<vista::Vertex>& src = core.vertex_list();
  if (!out || pad <= 0.f || src.empty()) {
    out->push_back(std::move(core));
    return;
  }
  PlacedMesh feather;
  feather.source = MeshSource::kSolid;
  feather.stroke = core.stroke;
  feather.vertices = src;
  expand_from_center(&feather.vertices, pad);
  feather.indices_src = core.indices_src;
  if (!feather.indices_src) {
    feather.indices = core.indices;
  }
  feather.r = core.r;
  feather.g = core.g;
  feather.b = core.b;
  feather.a = core.a * kFeatherAlphaScale;
  out->push_back(std::move(feather));
  out->push_back(std::move(core));
}

// Pixel quads stored with pixel_space left false are placed as lon/lat.
// A 16px glyph then covers tens of degrees: white halo, screen-sized
// characters, while the map underneath is only partly in frame.
bool text_quad_is_unplaced_pixels(const vista::DrawItem& item,
                                  const vista::View& view) {
  if (item.pixel_space || item.text_size_px <= 0.f || item.vertices.size() < 2 ||
      view.width_px == 0 || view.max_x <= view.min_x) {
    return false;
  }
  float min_x = item.vertices[0].x;
  float max_x = min_x;
  float min_y = item.vertices[0].y;
  float max_y = min_y;
  for (const vista::Vertex& v : item.vertices) {
    min_x = (std::min)(min_x, v.x);
    max_x = (std::max)(max_x, v.x);
    min_y = (std::min)(min_y, v.y);
    max_y = (std::max)(max_y, v.y);
  }
  const float edge = (std::max)(max_x - min_x, max_y - min_y);
  const float wupp = static_cast<float>(layout_world_per_pixel(view));
  if (!(wupp > 0.f) || !(edge > 4.f)) {
    return false;
  }
  const float expect = item.text_size_px * wupp;
  return edge > 4.f * (std::max)(expect, 1.0e-4f);
}

}  // namespace

// A glyph baked as raw pixels (or a stale ortho) covers a large fraction of
// the view: white halo, screen-sized characters, land only at the edges.
bool glyph_world_span_sane(const std::vector<vista::Vertex>& verts,
                           const vista::View& view, float /*text_size_px*/) {
  if (verts.size() < 2 || view.width_px == 0 || view.max_x <= view.min_x) {
    return false;
  }
  float min_x = verts[0].x;
  float max_x = min_x;
  float min_y = verts[0].y;
  float max_y = min_y;
  for (const vista::Vertex& v : verts) {
    min_x = (std::min)(min_x, v.x);
    max_x = (std::max)(max_x, v.x);
    min_y = (std::min)(min_y, v.y);
    max_y = (std::max)(max_y, v.y);
  }
  const float edge = (std::max)(max_x - min_x, max_y - min_y);
  const float wupp = static_cast<float>(layout_world_per_pixel(view));
  if (!(wupp > 0.f) || !(edge > 0.f)) {
    return false;
  }
  // Point labels run up to ~96px plus the atlas halo pad. Larger than this
  // is a stale pixel bake left in lon/lat, not a label.
  constexpr float kMaxGlyphPx = 192.f;
  return edge <= kMaxGlyphPx * wupp;
}

void seal_borrowed_meshes(std::vector<PlacedMesh>* meshes) {
  if (!meshes) {
    return;
  }
  for (PlacedMesh& mesh : *meshes) {
    if (mesh.vertices_src) {
      mesh.vertices = *mesh.vertices_src;
      mesh.vertices_src = nullptr;
    }
    if (mesh.indices_src) {
      mesh.indices = *mesh.indices_src;
      mesh.indices_src = nullptr;
    }
  }
}

std::vector<PlacedMesh> place_frame(const vista::MapIR& frame,
                                    const vista::View& view,
                                    bool world_items, bool overlay_items) {
  std::vector<PlacedMesh> out;
  if (!world_items && !overlay_items) {
    return out;
  }
  out.reserve(frame.items.size() * 2u);
  uint32_t next_group = 0;
  for (const vista::DrawItem& item : frame.items) {
    const bool kind_overlay = item.kind == vista::DrawKind::kIcon ||
                              item.kind == vista::DrawKind::kText;
    // Screen HUD rides the overlay lane so world-only interactive pan skips it.
    if (item.screen_fixed) {
      if (!overlay_items) {
        continue;
      }
    } else if (kind_overlay ? !overlay_items : !world_items) {
      continue;
    }
    // Skip insane meshes before indices_fit walks the index buffer.
    // Cap must match coalesce kMaxCoalesceVerts / kMaxCoalesceIndices.
    constexpr size_t kMaxPlaceVerts = 1u << 20;
    constexpr size_t kMaxPlaceIndices = 2u << 20;
    if (item.vertices.size() > kMaxPlaceVerts ||
        item.indices.size() > kMaxPlaceIndices) {
      std::fprintf(stderr,
                   "map2d: place skip oversized kind=%d verts=%zu indices=%zu "
                   "(cap %zu/%zu)\n",
                   static_cast<int>(item.kind), item.vertices.size(),
                   item.indices.size(), kMaxPlaceVerts, kMaxPlaceIndices);
      continue;
    }
    if (!indices_fit(item.indices, item.vertices.size())) {
      continue;
    }

    if (item.kind == vista::DrawKind::kText) {
      const bool as_pixels =
          item.pixel_space || text_quad_is_unplaced_pixels(item, view);
      if (as_pixels && !item.pixel_space) {
        std::fprintf(stderr,
                     "map2d: text quad coerced to pixel space size=%.1f "
                     "cp=%u\n",
                     item.text_size_px, item.codepoint);
      }
      std::vector<vista::Vertex> verts = item.vertices;
      const uint32_t group = ++next_group;
      // Atlas pads the cell by this many pixels and paints a glyph-shaped
      // halo there. Grow the quad by the same margin so the ink stays at
      // text_size and the plate is not an opaque rectangle over the terrain.
      constexpr float kGlyphHaloPadPx = 1.f;
      write_local_glyph_uv(as_pixels, &verts);
      if (item.halo_width_px > 0.f) {
        float cx = 0.f;
        float cy = 0.f;
        for (const vista::Vertex& v : verts) {
          cx += v.x;
          cy += v.y;
        }
        const float inv = 1.f / static_cast<float>(verts.size());
        cx *= inv;
        cy *= inv;
        for (vista::Vertex& v : verts) {
          if (v.x < cx) {
            v.x -= kGlyphHaloPadPx;
          } else if (v.x > cx) {
            v.x += kGlyphHaloPadPx;
          }
          if (v.y < cy) {
            v.y -= kGlyphHaloPadPx;
          } else if (v.y > cy) {
            v.y += kGlyphHaloPadPx;
          }
        }
      }
      if (as_pixels) {
        rotate_about_anchor(&verts, item.anchor_x, item.anchor_y,
                            item.angle_rad);
      }
      to_ortho(view, as_pixels, &verts);
      if (!glyph_world_span_sane(verts, view, item.text_size_px)) {
        std::fprintf(stderr,
                     "map2d: drop oversized text quad size=%.1f cp=%u\n",
                     item.text_size_px, item.codepoint);
        continue;
      }
      PlacedMesh mesh;
      mesh.source = MeshSource::kGlyph;
      mesh.overlay = true;
      mesh.screen_fixed = item.screen_fixed;
      mesh.vertices = std::move(verts);
      mesh.indices_src = &item.indices;
      tag_text(&mesh, item, group);
      out.push_back(std::move(mesh));
      continue;
    }

    if (item.kind == vista::DrawKind::kIcon) {
      std::vector<vista::Vertex> verts = item.vertices;
      if (item.pixel_space) {
        rotate_about_anchor(&verts, item.anchor_x, item.anchor_y,
                            item.angle_rad);
      }
      to_ortho(view, item.pixel_space, &verts);
      PlacedMesh mesh;
      mesh.source = MeshSource::kIcon;
      mesh.overlay = true;
      mesh.screen_fixed = item.screen_fixed;
      mesh.vertices = std::move(verts);
      mesh.indices_src = &item.indices;
      mesh.symbol_id = item.symbol_id;
      unpack_rgba(item.rgba, item.opacity, &mesh.r, &mesh.g, &mesh.b, &mesh.a);
      out.push_back(std::move(mesh));
      continue;
    }

    if (item.kind == vista::DrawKind::kRaster) {
      PlacedMesh mesh;
      mesh.source = MeshSource::kRaster;
      mesh.raster_key = item.codepoint;
      mesh.blend = item.blend;
      mesh.indices_src = &item.indices;
      unpack_rgba(item.rgba, item.opacity, &mesh.r, &mesh.g, &mesh.b, &mesh.a);
      if (item.pixel_space) {
        mesh.vertices = item.vertices;
        to_ortho(view, true, &mesh.vertices);
      } else {
        mesh.vertices_src = &item.vertices;
      }
      out.push_back(std::move(mesh));
      continue;
    }

    if (item.kind != vista::DrawKind::kFill &&
        item.kind != vista::DrawKind::kLine &&
        item.kind != vista::DrawKind::kCircle) {
      continue;
    }
    const bool patterned = !item.symbol_id.empty();
    const bool need_uv = patterned && uvs_unset(item.vertices);
    PlacedMesh mesh;
    mesh.indices_src = &item.indices;
    unpack_rgba(item.rgba, item.opacity, &mesh.r, &mesh.g, &mesh.b, &mesh.a);
    if (need_uv || item.pixel_space) {
      mesh.vertices = item.vertices;
      if (need_uv) {
        assign_bbox_uv(&mesh.vertices);
      }
      to_ortho(view, item.pixel_space, &mesh.vertices);
    } else {
      mesh.vertices_src = &item.vertices;
    }
    if (patterned) {
      mesh.source = MeshSource::kIcon;
      mesh.symbol_id = item.symbol_id;
      mesh.solid_if_icon_missing = true;
      out.push_back(std::move(mesh));
      continue;
    }
    mesh.source = MeshSource::kSolid;
    mesh.stroke = item.kind == vista::DrawKind::kLine ||
                  item.kind == vista::DrawKind::kCircle;
    if (item.screen_fixed) {
      mesh.overlay = true;
      mesh.screen_fixed = true;
      // HUD strokes are pixel-perfect; skip world AA feather under them.
      out.push_back(std::move(mesh));
      continue;
    }
    const float aa_pad = world_half_pixel(view);
    push_feather_under(std::move(mesh), aa_pad, &out);
  }
  return out;
}

}  // namespace detail
}  // namespace vista
