// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/map/layout/mesh_emit.h"

#include <cmath>
#include <string>

namespace vista {
namespace detail {

const SymbolAsset* find_symbol(const LayoutInput& in, const std::string& id) {
  if (id.empty()) {
    return nullptr;
  }
  for (const SymbolAsset& asset : in.symbols) {
    if (asset.id == id) {
      return &asset;
    }
  }
  return nullptr;
}

void push_quad(DrawItem* item, float x, float y, float w, float h, float u0,
               float v0, float u1, float v1) {
  const uint32_t base = static_cast<uint32_t>(item->vertices.size());
  item->vertices.push_back(Vertex{x, y, 0, u0, v0});
  item->vertices.push_back(Vertex{x + w, y, 0, u1, v0});
  item->vertices.push_back(Vertex{x + w, y + h, 0, u1, v1});
  item->vertices.push_back(Vertex{x, y + h, 0, u0, v1});
  item->indices.push_back(base);
  item->indices.push_back(base + 1);
  item->indices.push_back(base + 2);
  item->indices.push_back(base);
  item->indices.push_back(base + 2);
  item->indices.push_back(base + 3);
}

DrawItem mesh_item(const vista::TessMesh& mesh, DrawKind kind, uint32_t rgba,
                   float opacity) {
  DrawItem item;
  item.kind = kind;
  item.rgba = rgba;
  item.opacity = opacity;
  item.pixel_space = false;
  const size_t count = mesh.positions.size() / 3;
  item.vertices.resize(count);
  for (size_t i = 0; i < count; ++i) {
    item.vertices[i].x = mesh.positions[i * 3];
    item.vertices[i].y = mesh.positions[i * 3 + 1];
    item.vertices[i].z = mesh.positions[i * 3 + 2];
  }
  item.indices = mesh.indices;
  return item;
}

LineCap parse_cap(const std::string& cap) {
  if (cap == "round") {
    return LineCap::kRound;
  }
  if (cap == "square") {
    return LineCap::kSquare;
  }
  return LineCap::kButt;
}

LineJoin parse_join(const std::string& join) {
  if (join == "bevel") {
    return LineJoin::kBevel;
  }
  if (join == "round") {
    return LineJoin::kRound;
  }
  return LineJoin::kMiter;
}

LineTessOptions line_options(const gis::style::ResolvedPaint& paint,
                             double wupp) {
  LineTessOptions opts;
  opts.cap = parse_cap(paint.line_cap);
  opts.join = parse_join(paint.line_join);
  opts.pixel_width = paint.line_width;
  opts.world_units_per_pixel = wupp;
  // Default round fans are expensive; thin strokes use fewer wedges.
  if (paint.line_width <= 2.f) {
    opts.round_segments = 4;
  } else if (paint.line_width <= 4.f) {
    opts.round_segments = 6;
  }
  if (wupp > 0) {
    opts.dasharray.reserve(paint.line_dasharray.size());
    for (float dash : paint.line_dasharray) {
      opts.dasharray.push_back(static_cast<double>(dash) * wupp);
    }
  }
  return opts;
}

void emit_circle(DrawItem* item, double cx, double cy, double radius) {
  item->vertices.resize(static_cast<size_t>(kCircleSegments) + 1);
  item->vertices[0] = Vertex{static_cast<float>(cx), static_cast<float>(cy), 0,
                             0, 0};
  for (int i = 0; i < kCircleSegments; ++i) {
    const float a =
        (2.f * kPi * static_cast<float>(i)) / static_cast<float>(kCircleSegments);
    item->vertices[static_cast<size_t>(i) + 1] = Vertex{
        static_cast<float>(cx + radius * std::cos(a)),
        static_cast<float>(cy + radius * std::sin(a)), 0, 0, 0};
  }
  item->indices.reserve(static_cast<size_t>(kCircleSegments) * 3);
  for (int i = 0; i < kCircleSegments; ++i) {
    const int next = (i + 1) % kCircleSegments;
    item->indices.push_back(0);
    item->indices.push_back(static_cast<uint32_t>(i + 1));
    item->indices.push_back(static_cast<uint32_t>(next + 1));
  }
}

}  // namespace detail
}  // namespace vista
