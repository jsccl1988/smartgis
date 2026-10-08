// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/layout/raster.h"

#include <cstdio>

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
    std::fprintf(stderr, "map2d: emit_hillshade skip - no tiles\n");
    return;
  }
  gis::style::ResolvedPaint paint;
  gis::style::fill_resolved_paint(layer, nullptr, {}, in.zoom, &paint);
  for (const TileSlot& tile : in.hillshade_tiles) {
    DrawItem item;
    item.kind = DrawKind::kRaster;
    const auto ramp = layer.paint.find("hillshade-color-ramp");
    const bool jet =
        ramp != layer.paint.end() && ramp->second == "jet";
    item.blend = jet ? DrawBlend::kOver : DrawBlend::kMultiply;
    item.pixel_space = false;
    item.opacity = tile.opacity;
    item.rgba = 0xffffffffu;
    item.codepoint = tile.texture_key;
    const float x0 = static_cast<float>(tile.min_x);
    const float x1 = static_cast<float>(tile.max_x);
    const float y0 = static_cast<float>(tile.min_y);
    const float y1 = static_cast<float>(tile.max_y);
    // Bake RGBA is row0=north. GDI blit ignores UV and StretchBlts row0 to
    // the screen-top of the AABB (max_y). FlyCube upload presents that
    // buffer with v=0 at the opposite edge from land/ortho, so v=0@max_y
    // drew the sheet south-up (cream land ghost north of the jet mass).
    // Bind v=1 to max_y so north texels sit on the north of the view.
    item.vertices.push_back(Vertex{x0, y1, 0, 0, 1});
    item.vertices.push_back(Vertex{x1, y1, 0, 1, 1});
    item.vertices.push_back(Vertex{x1, y0, 0, 1, 0});
    item.vertices.push_back(Vertex{x0, y0, 0, 0, 0});
    item.indices = {0, 1, 2, 0, 2, 3};
    frame->items.push_back(std::move(item));
  }
  std::fprintf(stderr, "map2d: emit_hillshade tiles=%zu frame_items=%zu\n",
               in.hillshade_tiles.size(), frame->items.size());
  (void)paint;
}

}  // namespace detail
}  // namespace vista
