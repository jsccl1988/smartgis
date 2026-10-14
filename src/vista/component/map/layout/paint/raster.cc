// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/layout/paint/raster.h"

#include <algorithm>

#include "base/core/log.h"
#include "gis/style/paint_resolve.h"
#include "gis/style/style_types.h"

namespace vista {
namespace detail {

void apply_background(const gis::style::StyleLayer& layer, double zoom,
                      MapIR* frame) {
  if (!frame) {
    return;
  }
  // ResolvedPaint defaults background_color to opaque black. A failed parse
  // must not overwrite MapIR's carto ocean/cream clear — that turns jet
  // ocean (a=0) into a black hole under the DEM sheet.
  const auto found = layer.paint.find("background-color");
  if (found == layer.paint.end() || found->second.empty()) {
    return;
  }
  uint32_t argb = 0;
  if (!gis::style::parse_color(found->second, &argb)) {
    return;
  }
  gis::style::ResolvedPaint paint;
  gis::style::fill_resolved_paint(layer, nullptr, {}, zoom, &paint);
  frame->background_rgba = argb;
  frame->background_opacity = paint.background_opacity;
}

void emit_raster(const gis::style::StyleLayer& layer, const LayoutInput& in,
                 MapIR* frame) {
  gis::style::ResolvedPaint paint;
  gis::style::fill_resolved_paint(layer, nullptr, {}, in.zoom, &paint);
  for (const TileSlot& tile : in.tiles) {
    DrawItem item;
    item.kind = DrawKind::kRaster;
    item.pixel_space = false;
    item.opacity = tile.opacity * paint.raster_opacity;
    item.rgba = 0xffffffffu;
    item.codepoint = tile.texture_key;
    const float x0 = static_cast<float>(tile.min_x);
    const float x1 = static_cast<float>(tile.max_x);
    const float y0 = static_cast<float>(tile.min_y);
    const float y1 = static_cast<float>(tile.max_y);
    item.vertices.push_back(Vertex{x0, y1, 0, 0, 0});
    item.vertices.push_back(Vertex{x1, y1, 0, 1, 0});
    item.vertices.push_back(Vertex{x1, y0, 0, 1, 1});
    item.vertices.push_back(Vertex{x0, y0, 0, 0, 1});
    item.indices = {0, 1, 2, 0, 2, 3};
    frame->items.push_back(std::move(item));
  }
}

void emit_hillshade(const gis::style::StyleLayer& layer, const LayoutInput& in,
                    MapIR* frame) {
  if (in.hillshade_tiles.empty()) {
    LOGGING(LOG_INFO, "map2d: emit_hillshade skip - no tiles");
    return;
  }
  gis::style::ResolvedPaint paint;
  gis::style::fill_resolved_paint(layer, nullptr, {}, in.zoom, &paint);
  for (const TileSlot& tile : in.hillshade_tiles) {
    DrawItem item;
    item.kind = DrawKind::kRaster;
    // Product china DEM uses jet/HSHD and must composite with kOver. Multiply
    // coverage bake turns the sheet into grayscale bands that read as
    // horizontal streaks over cream land on FlyCube present.
    item.blend = DrawBlend::kOver;
    item.pixel_space = false;
    item.opacity = tile.opacity;
    item.rgba = 0xffffffffu;
    item.codepoint = tile.texture_key;
    // Clip the DEM quad to the view and remap UVs so FlyCube does not
    // clip a continent-sized triangle (large clip ranges corrupted UV
    // interpolation on DX12 and read as horizontal gray streaks).
    const double tw = tile.max_x - tile.min_x;
    const double th = tile.max_y - tile.min_y;
    if (tw <= 0.0 || th <= 0.0) {
      continue;
    }
    const double x0d = (std::max)(tile.min_x, in.view.min_x);
    const double x1d = (std::min)(tile.max_x, in.view.max_x);
    const double y0d = (std::max)(tile.min_y, in.view.min_y);
    const double y1d = (std::min)(tile.max_y, in.view.max_y);
    if (x1d <= x0d || y1d <= y0d) {
      continue;
    }
    const float x0 = static_cast<float>(x0d);
    const float x1 = static_cast<float>(x1d);
    const float y0 = static_cast<float>(y0d);
    const float y1 = static_cast<float>(y1d);
    const float u0 = static_cast<float>((x0d - tile.min_x) / tw);
    const float u1 = static_cast<float>((x1d - tile.min_x) / tw);
    // Bake row0 = north = tile.max_y => v=0; south max_y->min_y => v grows.
    const float v0 = static_cast<float>((tile.max_y - y1d) / th);
    const float v1 = static_cast<float>((tile.max_y - y0d) / th);
    item.vertices.push_back(Vertex{x0, y1, 0, u0, v0});
    item.vertices.push_back(Vertex{x1, y1, 0, u1, v0});
    item.vertices.push_back(Vertex{x1, y0, 0, u1, v1});
    item.vertices.push_back(Vertex{x0, y0, 0, u0, v1});
    item.indices = {0, 1, 2, 0, 2, 3};
    frame->items.push_back(std::move(item));
  }
  LOGGING(LOG_INFO, "map2d: emit_hillshade tiles=%zu frame_items=%zu",
          in.hillshade_tiles.size(), frame->items.size());
  (void)paint;
}

}  // namespace detail
}  // namespace vista
