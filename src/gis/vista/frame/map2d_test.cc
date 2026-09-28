// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "gis/vista/frame/detail/collision.h"
#include "gis/vista/frame/frame.h"
#include "gis/present/style/paint_resolve.h"
#include "gis/present/style/style_document.h"
#include "ogrsf_frmts.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

bool almost_eq(float a, float b) { return std::fabs(a - b) < 0.05f; }

class FixedAdvance : public gis::vista::GlyphMetrics {
 public:
  float advance_px(uint32_t, float text_size_px) const override {
    return text_size_px * 0.5f;
  }
};

gis::vista::View square_view(uint32_t px, double span) {
  gis::vista::View view;
  view.width_px = px;
  view.height_px = px;
  view.min_x = 0;
  view.min_y = 0;
  view.max_x = span;
  view.max_y = span;
  return view;
}

bool parse_style(const std::string& json, gis::style::StyleDocument* doc) {
  return gis::style::parse_style_document(json, doc);
}

int count_kind(const gis::vista::MapFrame& frame, gis::vista::DrawKind kind) {
  int n = 0;
  for (const auto& item : frame.items) {
    if (item.kind == kind) {
      ++n;
    }
  }
  return n;
}

}  // namespace

int main() {
  using gis::vista::DrawKind;
  using gis::vista::LayerBatch;
  using gis::vista::Layout;
  using gis::vista::LayoutInput;
  using gis::vista::TileSlot;

  const Layout layout;
  const FixedAdvance metrics;

  // Empty input still carries the default background and no meshes.
  {
    const gis::vista::MapFrame frame = layout.build({}, {});
    expect(frame.items.empty(), "empty frame keeps background");
    expect(frame.background_rgba == 0xfff5f0e6u, "empty frame keeps background");
  }

  // Painter order: background is a clear; fill is emitted before line.
  {
    gis::style::StyleDocument doc;
    expect(parse_style(
               "{"
               "\"version\":8,\"layers\":["
               "{\"id\":\"bg\",\"type\":\"background\",\"paint\":{"
               "\"background-color\":\"#112233\",\"background-opacity\":0.8}},"
               "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":\"land\","
               "\"paint\":{\"fill-color\":\"#f5f3e9\",\"fill-opacity\":1}},"
               "{\"id\":\"road\",\"type\":\"line\",\"source-layer\":\"road\","
               "\"paint\":{\"line-color\":\"#ffffff\",\"line-width\":2}}"
               "]}",
               &doc),
           "painter order fill before line");
    OGRLinearRing ring;
    ring.addPoint(1, 1);
    ring.addPoint(3, 1);
    ring.addPoint(2, 3);
    ring.closeRings();
    OGRPolygon poly;
    poly.addRing(&ring);
    OGRLineString road;
    road.addPoint(0, 0);
    road.addPoint(4, 0);
    LayerBatch land;
    land.source_layer = "land";
    land.geoms.push_back(&poly);
    LayerBatch roads;
    roads.source_layer = "road";
    roads.geoms.push_back(&road);
    LayoutInput in;
    in.view = square_view(100, 10);
    in.style = &doc;
    in.zoom = 8;
    const gis::vista::MapFrame frame = layout.build(in, {land, roads});
    expect(frame.background_rgba == 0xff112233u, "painter background is not a mesh");
    expect(almost_eq(frame.background_opacity, 0.8f), "painter background is not a mesh");
    expect(frame.items.size() == 2, "painter order fill before line");
    expect(frame.items.size() == 2 && frame.items[0].kind == DrawKind::kFill &&
               frame.items[1].kind == DrawKind::kLine,
           "painter order fill before line");
    expect(frame.items.size() == 2 && !frame.items[0].pixel_space &&
               !frame.items[1].pixel_space,
           "painter order fill before line");
  }

  // Two labels on the same point: the higher priority (lower score) stays.
  {
    gis::style::StyleDocument doc;
    expect(parse_style(
               "{"
               "\"version\":8,\"layers\":[{"
               "\"id\":\"label\",\"type\":\"symbol\",\"source-layer\":\"label\","
               "\"layout\":{\"symbol-placement\":\"point\",\"text-field\":\"{name}\","
               "\"text-size\":16,\"text-anchor\":\"center\"}"
               "}]}",
               &doc),
           "overlapping labels drop the lower priority");
    OGRPoint shop_pt(5, 5);
    OGRPoint title_pt(5, 5);
    LayerBatch labels;
    labels.source_layer = "label";
    labels.geoms.push_back(&shop_pt);
    labels.geoms.push_back(&title_pt);
    gis::style::AttrMap shop_attrs;
    shop_attrs["name"] = "Shop";
    gis::style::AttrMap title_attrs;
    title_attrs["name"] = "Capital";
    title_attrs["class"] = "title";
    labels.attrs.push_back(shop_attrs);
    labels.attrs.push_back(title_attrs);
    LayoutInput in;
    in.view = square_view(800, 10);
    in.style = &doc;
    in.zoom = 10;
    in.metrics = &metrics;
    const gis::vista::MapFrame frame = layout.build(in, {labels});
    expect(count_kind(frame, DrawKind::kText) == 7,
           "overlapping labels drop the lower priority");
    expect(!frame.items.empty() && frame.items[0].kind == DrawKind::kText &&
               frame.items[0].codepoint == static_cast<uint32_t>('C'),
           "overlapping labels drop the lower priority");
  }

  // Line placement takes the midpoint tangent in screen space (y down).
  {
    gis::style::StyleDocument doc;
    expect(parse_style(
               "{"
               "\"version\":8,\"layers\":[{"
               "\"id\":\"road-label\",\"type\":\"symbol\",\"source-layer\":\"road\","
               "\"layout\":{\"symbol-placement\":\"line\",\"text-field\":\"{name}\","
               "\"text-size\":14,\"text-anchor\":\"center\"}"
               "}]}",
               &doc),
           "along-line angle");
    OGRLineString line;
    line.addPoint(1, 1);
    line.addPoint(9, 9);
    LayerBatch roads;
    roads.source_layer = "road";
    roads.geoms.push_back(&line);
    gis::style::AttrMap road_attrs;
    road_attrs["name"] = "Road";
    road_attrs["class"] = "title";
    roads.attrs.push_back(road_attrs);
    LayoutInput in;
    in.view = square_view(200, 10);
    in.style = &doc;
    in.zoom = 10;
    in.metrics = &metrics;
    const gis::vista::MapFrame frame = layout.build(in, {roads});
    expect(count_kind(frame, DrawKind::kText) == 4, "along-line angle");
    bool angled = false;
    for (const auto& item : frame.items) {
      if (item.kind == DrawKind::kText && item.pixel_space &&
          almost_eq(item.angle_rad, -3.14159265f / 4.f)) {
        angled = true;
      }
    }
    expect(angled, "along-line angle");
  }

  // Horizontal along-line labels stay near 0 rad (screen y down, upright).
  {
    gis::style::StyleDocument doc;
    expect(parse_style(
               "{"
               "\"version\":8,\"layers\":[{"
               "\"id\":\"road-label\",\"type\":\"symbol\",\"source-layer\":\"road\","
               "\"layout\":{\"symbol-placement\":\"line\",\"text-field\":\"{name}\","
               "\"text-size\":14,\"text-anchor\":\"center\"}"
               "}]}",
               &doc),
           "along-line angle sign");
    OGRLineString line;
    line.addPoint(1, 5);
    line.addPoint(9, 5);
    LayerBatch roads;
    roads.source_layer = "road";
    roads.geoms.push_back(&line);
    gis::style::AttrMap attrs;
    attrs["name"] = "E-W";
    roads.attrs.push_back(attrs);
    LayoutInput in;
    in.view = square_view(200, 10);
    in.style = &doc;
    in.zoom = 10;
    in.metrics = &metrics;
    const gis::vista::MapFrame frame = layout.build(in, {roads});
    bool flat = false;
    for (const auto& item : frame.items) {
      if (item.kind == DrawKind::kText && almost_eq(item.angle_rad, 0.f)) {
        flat = true;
      }
    }
    expect(flat, "along-line angle sign");
  }

  // Style text-halo-width flows to DrawItem; omitted width uses carto halo_px.
  {
    gis::style::StyleDocument styled;
    expect(parse_style(
               "{"
               "\"version\":8,\"layers\":[{"
               "\"id\":\"label\",\"type\":\"symbol\",\"source-layer\":\"label\","
               "\"layout\":{\"text-field\":\"{name}\",\"text-size\":16,"
               "\"text-anchor\":\"center\"},"
               "\"paint\":{\"text-halo-color\":\"#ffffff\",\"text-halo-width\":3}"
               "}]}",
               &styled),
           "style halo width on glyphs");
    OGRPoint pt(5, 5);
    LayerBatch labels;
    labels.source_layer = "label";
    labels.geoms.push_back(&pt);
    gis::style::AttrMap attrs;
    attrs["name"] = "Hi";
    attrs["class"] = "title";
    labels.attrs.push_back(attrs);
    LayoutInput in;
    in.view = square_view(800, 10);
    in.style = &styled;
    in.zoom = 10;
    in.metrics = &metrics;
    const gis::vista::MapFrame styled_frame = layout.build(in, {labels});
    expect(!styled_frame.items.empty() &&
               styled_frame.items[0].halo_width_px == 3.f &&
               styled_frame.items[0].halo_rgba == 0xffffffffu,
           "style halo width on glyphs");

    gis::style::StyleDocument carto;
    expect(parse_style(
               "{"
               "\"version\":8,\"layers\":[{"
               "\"id\":\"label\",\"type\":\"symbol\",\"source-layer\":\"label\","
               "\"layout\":{\"text-field\":\"{name}\",\"text-size\":16,"
               "\"text-anchor\":\"center\"}"
               "}]}",
               &carto),
           "carto halo when width omitted");
    in.style = &carto;
    const gis::vista::MapFrame carto_frame = layout.build(in, {labels});
    expect(!carto_frame.items.empty() &&
               carto_frame.items[0].halo_width_px ==
                   static_cast<float>(gis::vista::detail::halo_px(0)) &&
               carto_frame.items[0].halo_rgba == 0xffffffffu,
           "carto halo when width omitted");
  }

  // Collision grid drops a second overlapping label at the same anchor.
  {
    gis::style::StyleDocument doc;
    expect(parse_style(
               "{"
               "\"version\":8,\"layers\":[{"
               "\"id\":\"label\",\"type\":\"symbol\",\"source-layer\":\"label\","
               "\"layout\":{\"text-field\":\"{name}\",\"text-size\":16,"
               "\"text-anchor\":\"center\"}"
               "}]}",
               &doc),
           "overlap drop with halo padding");
    OGRPoint shared(5, 5);
    LayerBatch labels;
    labels.source_layer = "label";
    labels.geoms.push_back(&shared);
    labels.geoms.push_back(&shared);
    gis::style::AttrMap a;
    a["name"] = "Alpha";
    a["class"] = "title";
    gis::style::AttrMap b;
    b["name"] = "Beta";
    labels.attrs.push_back(a);
    labels.attrs.push_back(b);
    LayoutInput in;
    in.view = square_view(800, 10);
    in.style = &doc;
    in.zoom = 10;
    in.metrics = &metrics;
    const gis::vista::MapFrame frame = layout.build(in, {labels});
    expect(count_kind(frame, DrawKind::kText) == 5, "overlap drop with halo padding");
  }

  // Circle is a triangle fan, not the old four-vertex diamond.
  {
    gis::style::StyleDocument doc;
    expect(parse_style(
               "{"
               "\"version\":8,\"layers\":[{"
               "\"id\":\"poi\",\"type\":\"circle\",\"source-layer\":\"poi\","
               "\"paint\":{\"circle-color\":\"#2244aa\",\"circle-radius\":8,"
               "\"circle-opacity\":0.9}"
               "}]}",
               &doc),
           "circle has more than 4 vertices");
    OGRPoint pt(5, 5);
    LayerBatch pois;
    pois.source_layer = "poi";
    pois.geoms.push_back(&pt);
    LayoutInput in;
    in.view = square_view(100, 10);
    in.style = &doc;
    in.zoom = 12;
    const gis::vista::MapFrame frame = layout.build(in, {pois});
    expect(frame.items.size() == 1 && frame.items[0].kind == DrawKind::kCircle &&
               frame.items[0].vertices.size() > 4 && !frame.items[0].pixel_space,
           "circle has more than 4 vertices");
  }

  // line-width pixels become a world ribbon via world-units-per-pixel.
  {
    gis::style::StyleDocument doc;
    expect(parse_style(
               "{"
               "\"version\":8,\"layers\":[{"
               "\"id\":\"road\",\"type\":\"line\",\"source-layer\":\"road\","
               "\"paint\":{\"line-color\":\"#ffffff\",\"line-width\":4},"
               "\"layout\":{\"line-cap\":\"butt\",\"line-join\":\"miter\"}"
               "}]}",
               &doc),
           "line width passed through");
    OGRLineString seg;
    seg.addPoint(10, 50);
    seg.addPoint(90, 50);
    LayerBatch roads;
    roads.source_layer = "road";
    roads.geoms.push_back(&seg);
    LayoutInput in;
    in.view = square_view(100, 100);
    in.style = &doc;
    in.zoom = 8;
    const gis::vista::MapFrame frame = layout.build(in, {roads});
    expect(frame.items.size() == 1 && frame.items[0].kind == DrawKind::kLine,
           "line width passed through");
    float max_dy = 0.f;
    if (!frame.items.empty()) {
      for (const auto& v : frame.items[0].vertices) {
        max_dy = (std::max)(max_dy, std::fabs(v.y - 50.f));
      }
    }
    expect(almost_eq(max_dy, 2.f), "line width passed through");
  }

  // Raster quad stays in lon/lat. texture_key rides in codepoint.
  {
    gis::style::StyleDocument doc;
    expect(parse_style(
               "{"
               "\"version\":8,\"layers\":[{"
               "\"id\":\"sat\",\"type\":\"raster\","
               "\"paint\":{\"raster-opacity\":0.5}"
               "}]}",
               &doc),
           "raster quad in lon/lat");
    TileSlot tile;
    tile.min_x = 1;
    tile.min_y = 2;
    tile.max_x = 3;
    tile.max_y = 4;
    tile.opacity = 0.5f;
    tile.texture_key = 42;
    LayoutInput in;
    in.view = square_view(64, 10);
    in.style = &doc;
    in.zoom = 3;
    in.tiles.push_back(tile);
    const gis::vista::MapFrame frame = layout.build(in, {});
    expect(frame.items.size() == 1 && frame.items[0].kind == DrawKind::kRaster &&
               frame.items[0].vertices.size() == 4 &&
               frame.items[0].indices.size() == 6 &&
               frame.items[0].codepoint == 42 && frame.items[0].symbol_id.empty() &&
               !frame.items[0].pixel_space,
           "raster quad in lon/lat");
    if (frame.items.size() == 1) {
      const auto& v = frame.items[0].vertices;
      expect(almost_eq(v[0].x, 1.f) && almost_eq(v[0].y, 4.f) && almost_eq(v[0].u, 0.f) &&
                 almost_eq(v[0].v, 0.f),
             "raster quad in lon/lat");
      expect(almost_eq(v[2].x, 3.f) && almost_eq(v[2].y, 2.f) && almost_eq(v[2].u, 1.f) &&
                 almost_eq(v[2].v, 1.f),
             "raster quad in lon/lat");
    }
  }

  // Missing icon still places text. Missing metrics skips text and keeps the icon.
  {
    gis::style::StyleDocument doc;
    expect(parse_style(
               "{"
               "\"version\":8,\"layers\":[{"
               "\"id\":\"label\",\"type\":\"symbol\",\"source-layer\":\"label\","
               "\"layout\":{\"text-field\":\"{name}\",\"text-size\":16,"
               "\"icon-image\":\"pin\",\"icon-size\":1}"
               "}]}",
               &doc),
           "missing icon still places text");
    OGRPoint pt(5, 5);
    LayerBatch labels;
    labels.source_layer = "label";
    labels.geoms.push_back(&pt);
    gis::style::AttrMap hi_attrs;
    hi_attrs["name"] = "Hi";
    hi_attrs["class"] = "title";
    labels.attrs.push_back(hi_attrs);
    LayoutInput in;
    in.view = square_view(800, 10);
    in.style = &doc;
    in.zoom = 10;
    in.metrics = &metrics;
    const gis::vista::MapFrame text_only = layout.build(in, {labels});
    expect(count_kind(text_only, DrawKind::kIcon) == 0 &&
               count_kind(text_only, DrawKind::kText) == 2,
           "missing icon still places text");
    in.metrics = nullptr;
    in.symbols.push_back(gis::vista::SymbolAsset{"pin", 16.f, 16.f});
    const gis::vista::MapFrame icon_only = layout.build(in, {labels});
    expect(count_kind(icon_only, DrawKind::kText) == 0 &&
               count_kind(icon_only, DrawKind::kIcon) == 1 &&
               icon_only.items[0].symbol_id == "pin",
           "missing metrics skips text");
  }

  // Embedded cartography document parses and keeps casing before fill.
  {
    gis::style::StyleDocument doc;
    const std::string json = gis::vista::default_carto_style_json();
    expect(parse_style(json, &doc), "default style JSON parses");
    expect(doc.layers.size() == 10, "default style JSON parses");
    int casing = -1;
    int fill = -1;
    for (int i = 0; i < static_cast<int>(doc.layers.size()); ++i) {
      if (doc.layers[static_cast<size_t>(i)].id == "road-casing") {
        casing = i;
      }
      if (doc.layers[static_cast<size_t>(i)].id == "road") {
        fill = i;
      }
    }
    expect(casing >= 0 && fill > casing, "default style JSON parses");
    expect(doc.layers.size() == 10 &&
               doc.layers[0].type == gis::style::LayerType::kBackground &&
               doc.layers[1].type == gis::style::LayerType::kFill &&
               doc.layers[3].type == gis::style::LayerType::kLine &&
               doc.layers[8].id == "river-label" &&
               doc.layers[8].layout.at("symbol-placement") == "line" &&
               doc.layers[9].id == "road-label" &&
               doc.layers[9].layout.at("symbol-placement") == "line",
           "default style JSON parses");
    gis::style::ResolvedPaint paint;
    gis::style::fill_resolved_paint(doc.layers[9], nullptr, {}, 10, &paint);
    expect(paint.symbol_placement == "line" && almost_eq(paint.text_halo_width, 2.f) &&
               paint.text_halo_color == 0xffffffffu,
           "default style JSON parses");
    gis::style::ResolvedPaint point_paint;
    gis::style::fill_resolved_paint(doc.layers[7], nullptr, {}, 10, &point_paint);
    expect(almost_eq(point_paint.text_halo_width, 3.f), "default style JSON parses");
  }

  if (g_fails) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("map2d_test OK\n");
  return 0;
}
