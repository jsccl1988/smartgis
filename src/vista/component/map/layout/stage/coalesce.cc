// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/layout/coalesce.h"

namespace vista {
namespace detail {
namespace {

bool same_style(const DrawItem& a, const DrawItem& b) {
  if (a.kind != b.kind || a.kind == DrawKind::kRaster ||
      a.kind == DrawKind::kText || a.kind == DrawKind::kIcon) {
    return false;
  }
  return a.rgba == b.rgba && a.opacity == b.opacity && a.blend == b.blend &&
         a.symbol_id == b.symbol_id && a.pixel_space == b.pixel_space &&
         a.halo_width_px == b.halo_width_px && a.halo_rgba == b.halo_rgba &&
         a.text_size_px == b.text_size_px && a.codepoint == b.codepoint &&
         a.cache_key == b.cache_key;
}

void append_mesh(DrawItem* dst, const DrawItem& src) {
  const uint32_t base = static_cast<uint32_t>(dst->vertices.size());
  dst->vertices.insert(dst->vertices.end(), src.vertices.begin(),
                       src.vertices.end());
  dst->indices.reserve(dst->indices.size() + src.indices.size());
  for (uint32_t idx : src.indices) {
    dst->indices.push_back(base + idx);
  }
}

}  // namespace

void coalesce_draw_items(std::vector<DrawItem>* items) {
  if (!items || items->size() < 2) {
    return;
  }
  std::vector<DrawItem> out;
  out.reserve(items->size());
  for (DrawItem& item : *items) {
    if (!out.empty() && same_style(out.back(), item)) {
      append_mesh(&out.back(), item);
      continue;
    }
    out.push_back(std::move(item));
  }
  *items = std::move(out);
}

}  // namespace detail
}  // namespace vista
