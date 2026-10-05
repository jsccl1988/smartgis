// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "vista/component/map/detail/carto_filter.h"
#include "vista/component/map/detail/collision.h"
#include "vista/component/map/layout/slice_key.h"
#include "vista/component/map/ir.h"
#include "gis/style/paint_resolve.h"
#include "gis/style/document/style_document.h"
#include "gis/style/eval/style_rules.h"
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

class FixedAdvance : public vista::GlyphMetrics {
 public:
  float advance_px(uint32_t, float text_size_px) const override {
    return text_size_px * 0.5f;
  }
};

vista::View square_view(uint32_t px, double span) {
  vista::View view;
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

int count_kind(const vista::MapIR& frame, vista::DrawKind kind) {
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
  using vista::DrawKind;
  using vista::LayerBatch;
  using vista::Layout;
  using vista::LayoutInput;
  using vista::TileSlot;

  const Layout layout;
  const FixedAdvance metrics;

  // Empty input still carries the default background and no meshes.
  {
    const vista::MapIR frame = layout.build({}, {});
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
    const vista::MapIR frame = layout.build(in, {land, roads});
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
    const vista::MapIR frame = layout.build(in, {labels});
    const int texts = count_kind(frame, DrawKind::kText);
    expect(texts >= 7, "overlapping labels keep higher priority");
    expect(!frame.items.empty() && frame.items[0].kind == DrawKind::kText &&
               frame.items[0].codepoint == static_cast<uint32_t>('C'),
           "overlapping labels keep higher priority");
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
    const vista::MapIR frame = layout.build(in, {roads});
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
    attrs["name"] = "East";
    attrs["class"] = "title";
    roads.attrs.push_back(attrs);
    LayoutInput in;
    in.view = square_view(200, 10);
    in.style = &doc;
    in.zoom = 10;
    in.metrics = &metrics;
    const vista::MapIR frame = layout.build(in, {roads});
    bool flat = false;
    for (const auto& item : frame.items) {
      if (item.kind == DrawKind::kText && almost_eq(item.angle_rad, 0.f)) {
        flat = true;
      }
    }
    expect(flat, "along-line angle sign");
  }

  // Collision helpers: path length, pose fractions, mixed-script estimate.
  {
    const int xy[] = {0, 0, 100, 0, 100, 50};
    expect(almost_eq(vista::detail::line_path_length_px(xy, 3), 150.f),
           "line_path_length_px");
    vista::detail::LineLabelPose mid;
    vista::detail::LineLabelPose q;
    expect(vista::detail::line_label_pose_at(xy, 3, 0.5f, &mid),
           "pose_at mid");
    expect(vista::detail::line_label_pose_at(xy, 3, 0.25f, &q),
           "pose_at quarter");
    expect(mid.x == 75 && mid.y == 0, "pose_at mid xy");
    expect(q.x == 37 || q.x == 38, "pose_at quarter x");
    expect(vista::detail::line_fits_label(100.f, 80.f), "line_fits ok");
    expect(!vista::detail::line_fits_label(80.f, 100.f), "line_fits reject");
    float slots[8];
    const int n = vista::detail::line_label_slot_fractions(8, slots);
    expect(n >= 5 && almost_eq(slots[0], 0.5f), "slot fractions mid first");
    // Latin narrower than CJK at same unit count ("AB" vs two CJK).
    const float latin =
        vista::detail::estimate_run_width_px("AB", 20.f);
    const char cjk[] = "\xe4\xb8\xad\xe5\x9b\xbd";  // 涓浗
    const float wide = vista::detail::estimate_run_width_px(cjk, 20.f);
    expect(wide > latin * 1.4f, "CJK estimate wider than Latin");
    vista::detail::LabelBox upright;
    upright.left = 0;
    upright.top = 0;
    upright.right = 40;
    upright.bottom = 10;
    upright.priority = 1;
    const vista::detail::LabelBox rot =
        vista::detail::rotate_label_box(upright, 90.f);
    expect(rot.right - rot.left >= 10 && rot.bottom - rot.top >= 40,
           "rotate_label_box aabb");
  }

  // Point blocker at line midpoint: road label still places via alternate slot.
  {
    gis::style::StyleDocument doc;
    expect(parse_style(
               "{"
               "\"version\":8,\"layers\":["
               "{\"id\":\"blocker\",\"type\":\"symbol\",\"source-layer\":\"poi\","
               "\"layout\":{\"text-field\":\"{name}\",\"text-size\":20,"
               "\"text-anchor\":\"center\"}},"
               "{\"id\":\"road-label\",\"type\":\"symbol\",\"source-layer\":\"road\","
               "\"layout\":{\"symbol-placement\":\"line\",\"text-field\":\"{name}\","
               "\"text-size\":12,\"text-anchor\":\"center\"}}"
               "]}",
               &doc),
           "along-line slot retry");
    OGRPoint mid(5, 5);
    LayerBatch pois;
    pois.source_layer = "poi";
    pois.geoms.push_back(&mid);
    gis::style::AttrMap poi_attrs;
    poi_attrs["name"] = "BLOCK";
    poi_attrs["class"] = "title";
    pois.attrs.push_back(poi_attrs);
    OGRLineString line;
    line.addPoint(0, 5);
    line.addPoint(10, 5);
    LayerBatch roads;
    roads.source_layer = "road";
    roads.geoms.push_back(&line);
    gis::style::AttrMap road_attrs;
    road_attrs["name"] = "Rd";
    road_attrs["class"] = "title";
    roads.attrs.push_back(road_attrs);
    LayoutInput in;
    in.view = square_view(400, 10);
    in.style = &doc;
    in.zoom = 12;
    in.metrics = &metrics;
    const vista::MapIR frame = layout.build(in, {pois, roads});
    const int texts = count_kind(frame, DrawKind::kText);
    // BLOCK=5 glyphs + Rd=2 glyphs when both survive; mid collision alone
    // would keep only BLOCK (5). Slot retry must keep Rd as well.
    expect(texts >= 7, "along-line slot retry");
    bool saw_offset = false;
    for (const auto& item : frame.items) {
      if (item.kind == DrawKind::kText && item.codepoint == 'R') {
        // Midpoint of 0..10 at zoom view is x=200; alternate slots leave mid.
        if (std::fabs(item.anchor_x - 200.f) > 20.f) {
          saw_offset = true;
        }
      }
    }
    expect(saw_offset, "along-line slot offset from mid");
  }

  // Along-line text shorter than path width is dropped (icon-less).
  {
    gis::style::StyleDocument doc;
    expect(parse_style(
               "{"
               "\"version\":8,\"layers\":[{"
               "\"id\":\"road-label\",\"type\":\"symbol\",\"source-layer\":\"road\","
               "\"layout\":{\"symbol-placement\":\"line\",\"text-field\":\"{name}\","
               "\"text-size\":40,\"text-anchor\":\"center\"}"
               "}]}",
               &doc),
           "short path drops long label");
    OGRLineString stub;
    stub.addPoint(4.8, 5);
    stub.addPoint(5.2, 5);
    LayerBatch roads;
    roads.source_layer = "road";
    roads.geoms.push_back(&stub);
    gis::style::AttrMap attrs;
    attrs["name"] = "LONGLABEL";
    attrs["class"] = "title";
    roads.attrs.push_back(attrs);
    LayoutInput in;
    in.view = square_view(200, 10);
    in.style = &doc;
    in.zoom = 10;
    in.metrics = &metrics;
    const vista::MapIR frame = layout.build(in, {roads});
    expect(count_kind(frame, DrawKind::kText) == 0, "short path drops long label");
  }

  // Icon+text union: both kinds emit when a single packed box is kept.
  {
    gis::style::StyleDocument doc;
    expect(parse_style(
               "{"
               "\"version\":8,\"layers\":[{"
               "\"id\":\"label\",\"type\":\"symbol\",\"source-layer\":\"label\","
               "\"layout\":{\"text-field\":\"{name}\",\"text-size\":14,"
               "\"text-anchor\":\"left\",\"icon-image\":\"pin\",\"icon-size\":1}"
               "}]}",
               &doc),
           "icon+text packed box");
    OGRPoint pt(5, 5);
    LayerBatch labels;
    labels.source_layer = "label";
    labels.geoms.push_back(&pt);
    gis::style::AttrMap attrs;
    attrs["name"] = "P";
    attrs["class"] = "title";
    labels.attrs.push_back(attrs);
    LayoutInput in;
    in.view = square_view(400, 10);
    in.style = &doc;
    in.zoom = 10;
    in.metrics = &metrics;
    in.symbols.push_back(vista::SymbolAsset{"pin", 24.f, 24.f});
    const vista::MapIR frame = layout.build(in, {labels});
    expect(count_kind(frame, DrawKind::kIcon) == 1 &&
               count_kind(frame, DrawKind::kText) == 1,
           "icon+text packed box");
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
    const vista::MapIR styled_frame = layout.build(in, {labels});
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
    const vista::MapIR carto_frame = layout.build(in, {labels});
    expect(!carto_frame.items.empty() &&
               carto_frame.items[0].halo_width_px ==
                   static_cast<float>(vista::detail::halo_px(0)) &&
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
    const vista::MapIR frame = layout.build(in, {labels});
    expect(count_kind(frame, DrawKind::kText) >= 5,
           "overlap drop with halo padding");
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
    const vista::MapIR frame = layout.build(in, {pois});
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
    const vista::MapIR frame = layout.build(in, {roads});
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
    const vista::MapIR frame = layout.build(in, {});
    expect(frame.items.size() == 1 && frame.items[0].kind == DrawKind::kRaster &&
               frame.items[0].vertices.size() == 4 &&
               frame.items[0].indices.size() == 6 &&
               frame.items[0].codepoint == 42 && frame.items[0].symbol_id.empty() &&
               !frame.items[0].pixel_space &&
               frame.items[0].blend == vista::DrawBlend::kOver,
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
    const vista::MapIR text_only = layout.build(in, {labels});
    expect(count_kind(text_only, DrawKind::kIcon) == 0 &&
               count_kind(text_only, DrawKind::kText) == 2,
           "missing icon still places text");
    in.metrics = nullptr;
    in.symbols.push_back(vista::SymbolAsset{"pin", 16.f, 16.f});
    const vista::MapIR icon_only = layout.build(in, {labels});
    expect(count_kind(icon_only, DrawKind::kText) == 0 &&
               count_kind(icon_only, DrawKind::kIcon) == 1 &&
               icon_only.items[0].symbol_id == "pin",
           "missing metrics skips text");
  }

  // Parallel tess path (鈮? geoms / flattened MultiLineString parts) keeps counts.
  {
    gis::style::StyleDocument doc;
    expect(parse_style(
               "{"
               "\"version\":8,\"layers\":["
               "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":\"land\","
               "\"paint\":{\"fill-color\":\"#f5f3e9\",\"fill-opacity\":1}},"
               "{\"id\":\"road\",\"type\":\"line\",\"source-layer\":\"road\","
               "\"paint\":{\"line-color\":\"#ffffff\",\"line-width\":2}}"
               "]}",
               &doc),
           "parallel tess keeps draw counts");
    std::vector<OGRPolygon> polys(4);
    std::vector<OGRLinearRing> rings(4);
    LayerBatch land;
    land.source_layer = "land";
    for (size_t i = 0; i < polys.size(); ++i) {
      const double o = static_cast<double>(i) * 2.0;
      rings[i].addPoint(o, o);
      rings[i].addPoint(o + 1.0, o);
      rings[i].addPoint(o + 0.5, o + 1.0);
      rings[i].closeRings();
      polys[i].addRing(&rings[i]);
      land.geoms.push_back(&polys[i]);
    }
    OGRMultiLineString multi;
    for (int i = 0; i < 6; ++i) {
      OGRLineString* seg = new OGRLineString();
      seg->addPoint(i, 0);
      seg->addPoint(i, 5);
      multi.addGeometryDirectly(seg);
    }
    LayerBatch roads;
    roads.source_layer = "road";
    roads.geoms.push_back(&multi);
    LayoutInput in;
    in.view = square_view(200, 20);
    in.style = &doc;
    in.zoom = 8;
    const vista::MapIR frame = layout.build(in, {land, roads});
    expect(count_kind(frame, DrawKind::kFill) == 1,
           "parallel fill coalesces same-style polygons");
    expect(count_kind(frame, DrawKind::kLine) == 1,
           "parallel line coalesces MultiLineString parts");
  }

  // Hillshade layer emits a raster underlay when host supplies tiles.
  {
    gis::style::StyleDocument doc;
    expect(parse_style("{\"version\":8,\"layers\":["
                       "{\"id\":\"shade\",\"type\":\"hillshade\",\"paint\":{}},"
                       "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":"
                       "\"land\",\"paint\":{\"fill-color\":\"#f5f3e9\"}}"
                       "]}",
                       &doc),
           "hillshade style parses");
    vista::TileSlot hs;
    hs.min_x = 0;
    hs.min_y = 0;
    hs.max_x = 20;
    hs.max_y = 20;
    hs.opacity = 0.7f;
    hs.texture_key = 0x48534844u;
    LayoutInput in;
    in.view = square_view(200, 20);
    in.style = &doc;
    in.zoom = 8;
    in.hillshade_tiles = {hs};
    OGRPolygon land_poly;
    OGRLinearRing* ring = new OGRLinearRing();
    ring->addPoint(2, 2);
    ring->addPoint(18, 2);
    ring->addPoint(18, 18);
    ring->addPoint(2, 18);
    ring->addPoint(2, 2);
    land_poly.addRingDirectly(ring);
    LayerBatch land;
    land.source_layer = "land";
    land.geoms = {&land_poly};
    const vista::MapIR frame = layout.build(in, {land});
    expect(!frame.items.empty() && frame.items[0].kind == DrawKind::kRaster &&
               frame.items[0].codepoint == 0x48534844u &&
               frame.items[0].blend == vista::DrawBlend::kMultiply,
           "hillshade underlay first");
    expect(count_kind(frame, DrawKind::kFill) >= 1, "fill after hillshade");
  }

  // Land that sits outside the DEM slot is clipped (no cream fringe).
  {
    gis::style::StyleDocument doc;
    expect(parse_style("{\"version\":8,\"layers\":["
                       "{\"id\":\"shade\",\"type\":\"hillshade\",\"paint\":{}},"
                       "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":"
                       "\"land\",\"paint\":{\"fill-color\":\"#f5f3e9\"}}"
                       "]}",
                       &doc),
           "hillshade land-clip style");
    vista::TileSlot hs;
    hs.min_x = 0;
    hs.min_y = 0;
    hs.max_x = 5;
    hs.max_y = 5;
    hs.texture_key = 1;
    LayoutInput in;
    in.view = square_view(200, 20);
    in.style = &doc;
    in.zoom = 8;
    in.hillshade_tiles = {hs};
    OGRPolygon far;
    OGRLinearRing* ring = new OGRLinearRing();
    ring->addPoint(10, 10);
    ring->addPoint(18, 10);
    ring->addPoint(18, 18);
    ring->addPoint(10, 18);
    ring->addPoint(10, 10);
    far.addRingDirectly(ring);
    LayerBatch land;
    land.source_layer = "land";
    land.geoms = {&far};
    const vista::MapIR frame = layout.build(in, {land});
    expect(count_kind(frame, DrawKind::kFill) == 0,
           "land outside DEM slot dropped");
  }

  // Fill-extrusion v1: prism walls + roof DrawItems for a zoom-matched layer.
  {
    gis::style::StyleDocument doc;
    expect(parse_style("{\"version\":8,\"layers\":["
                       "{\"id\":\"bldg\",\"type\":\"fill-extrusion\","
                       "\"source-layer\":\"bldg\",\"paint\":{"
                       "\"fill-extrusion-height\":2,"
                       "\"fill-extrusion-base\":0,"
                       "\"fill-extrusion-color\":\"#6688aa\","
                       "\"fill-extrusion-opacity\":1}}]}",
                       &doc),
           "fill-extrusion style parses");
    LayoutInput in;
    in.view = square_view(200, 20);
    in.style = &doc;
    in.zoom = 14;
    OGRPolygon bldg;
    OGRLinearRing* ring = new OGRLinearRing();
    ring->addPoint(4, 4);
    ring->addPoint(8, 4);
    ring->addPoint(8, 8);
    ring->addPoint(4, 8);
    ring->addPoint(4, 4);
    bldg.addRingDirectly(ring);
    LayerBatch batch;
    batch.source_layer = "bldg";
    batch.geoms = {&bldg};
    const vista::MapIR frame = layout.build(in, {batch});
    expect(count_kind(frame, DrawKind::kFill) >= 2,
           "extrusion emits walls and roof");
    bool saw_lifted = false;
    for (const auto& item : frame.items) {
      for (const auto& v : item.vertices) {
        if (v.z > 1.5f) {
          saw_lifted = true;
        }
      }
    }
    expect(saw_lifted, "extrusion roof has height z");
  }

  // Heatmap v1: Style constants 鈫?circle splat DrawItems (zoom-matched).
  {
    gis::style::StyleDocument doc;
    expect(parse_style("{\"version\":8,\"layers\":["
                       "{\"id\":\"heat\",\"type\":\"heatmap\","
                       "\"source-layer\":\"heatmap\",\"minzoom\":5,"
                       "\"paint\":{"
                       "\"heatmap-radius\":10,"
                       "\"heatmap-weight\":1,"
                       "\"heatmap-intensity\":1,"
                       "\"heatmap-color\":\"#ff6400\","
                       "\"heatmap-opacity\":0.5}}]}",
                       &doc),
           "heatmap style parses");
    LayoutInput in;
    in.view = square_view(100, 10);
    in.style = &doc;
    in.zoom = 8;
    OGRPoint a(3, 3);
    OGRPoint b(7, 7);
    LayerBatch batch;
    batch.source_layer = "heatmap";
    batch.geoms = {&a, &b};
    const vista::MapIR frame = layout.build(in, {batch});
    expect(count_kind(frame, DrawKind::kCircle) == 1,
           "heatmap coalesces same-style splats");
    expect(!frame.items.empty() && frame.items[0].rgba == 0xFFFF6400u &&
               almost_eq(frame.items[0].opacity, 0.5f) &&
               frame.items[0].vertices.size() > 4,
           "heatmap splat color and opacity");

    in.zoom = 3;
    const vista::MapIR miss = layout.build(in, {batch});
    expect(miss.items.empty(), "heatmap respects minzoom");
  }

  // Embedded cartography document parses and keeps casing before fill.
  {
    gis::style::StyleDocument doc;
    const std::string json = vista::default_carto_style_json();
    expect(parse_style(json, &doc), "default style JSON parses");
    expect(doc.layers.size() == 12, "default style layer count");
    int casing = -1;
    int fill = -1;
    int hillshade = -1;
    int heatmap = -1;
    for (int i = 0; i < static_cast<int>(doc.layers.size()); ++i) {
      if (doc.layers[static_cast<size_t>(i)].id == "road-casing") {
        casing = i;
      }
      if (doc.layers[static_cast<size_t>(i)].id == "road") {
        fill = i;
      }
      if (doc.layers[static_cast<size_t>(i)].id == "hillshade") {
        hillshade = i;
      }
      if (doc.layers[static_cast<size_t>(i)].id == "heatmap") {
        heatmap = i;
      }
    }
    expect(casing >= 0 && fill > casing, "casing before road fill");
    expect(hillshade > 0 && hillshade < casing,
           "hillshade before roads");
    expect(heatmap > hillshade && heatmap < casing,
           "heatmap after hillshade before roads");
    expect(doc.layers[static_cast<size_t>(hillshade)].type ==
               gis::style::LayerType::kHillshade,
           "hillshade layer type");
    expect(doc.layers[static_cast<size_t>(heatmap)].type ==
               gis::style::LayerType::kHeatmap,
           "heatmap layer type");
    // Product zoom_from_scale puts china overview near ~11; road minzoom 5
    // matches MapLibre national band so dual-stroke casing paints.
    expect(gis::style::layer_matches_zoom(doc.layers[static_cast<size_t>(casing)],
                                           8.0),
           "road-casing visible at national-equivalent zoom");
    expect(gis::style::layer_matches_zoom(doc.layers[static_cast<size_t>(fill)],
                                           8.0),
           "road fill visible at national-equivalent zoom");
    expect(gis::style::layer_matches_zoom(doc.layers[static_cast<size_t>(casing)],
                                          12.0),
           "road-casing matches regional zoom");
    expect(gis::style::layer_matches_zoom(doc.layers[static_cast<size_t>(fill)],
                                          12.0),
           "road fill matches regional zoom");
    {
      const float casing_w = std::stof(
          doc.layers[static_cast<size_t>(casing)].paint.at("line-width"));
      const float fill_w = std::stof(
          doc.layers[static_cast<size_t>(fill)].paint.at("line-width"));
      expect(casing_w > fill_w, "casing wider than fill");
    }
    expect(doc.layers[0].type == gis::style::LayerType::kBackground &&
               doc.layers[1].type == gis::style::LayerType::kFill &&
               doc.layers[5].type == gis::style::LayerType::kLine &&
               doc.layers[10].id == "river-label" &&
               doc.layers[10].layout.at("symbol-placement") == "line" &&
               doc.layers[11].id == "road-label" &&
               doc.layers[11].layout.at("symbol-placement") == "line",
           "default style layer order");
    gis::style::ResolvedPaint paint;
    gis::style::fill_resolved_paint(doc.layers[11], nullptr, {}, 10, &paint);
    expect(paint.symbol_placement == "line" && almost_eq(paint.text_halo_width, 2.f) &&
               paint.text_halo_color == 0xffffffffu,
           "road-label paint");
    gis::style::ResolvedPaint point_paint;
    gis::style::fill_resolved_paint(doc.layers[9], nullptr, {}, 10, &point_paint);
    expect(almost_eq(point_paint.text_halo_width, 2.f),
           "label text-halo-width");
    gis::style::ResolvedPaint heat_paint;
    gis::style::fill_resolved_paint(doc.layers[static_cast<size_t>(heatmap)],
                                    nullptr, {}, 10, &heat_paint);
    expect(heat_paint.type == gis::style::LayerType::kHeatmap &&
               almost_eq(heat_paint.heatmap_radius, 24.f),
           "default heatmap paint");
  }

  {
    using vista::detail::PointNudge;
    expect(vista::detail::label_min_importance(12.0) == 2,
           "country scale floor");
    expect(vista::detail::label_min_importance(30.0) == 1, "mid scale floor");
    expect(vista::detail::label_min_importance(70.0) == 0, "close scale floor");
    expect(vista::detail::label_draw_budget(10.0) == 24, "budget country");
    expect(vista::detail::label_draw_budget(30.0) == 40, "budget mid");
    expect(vista::detail::label_draw_budget(80.0) == 80, "budget regional");
    expect(vista::detail::label_draw_budget(200.0) == 120, "budget close");
    PointNudge nudges[8];
    const int n = vista::detail::point_label_nudges(18, nudges, 8);
    expect(n == 8 && nudges[0].dx == 0 && nudges[0].dy == 0, "nudge anchor");
    expect(nudges[1].dx == 0 && nudges[1].dy == -(18 + 4), "nudge up");
    expect(nudges[2].dx == 14 && nudges[2].dy == 0, "nudge right");
    expect(nudges[3].dx == -14 && nudges[3].dy == 0, "nudge left");
    expect(nudges[7].dx == 0 && nudges[7].dy == 18 + 2, "nudge down");
  }

  // N1: cache_key is layer × world tile. Pan one column keeps unmoved tiles.
  {
    gis::style::StyleDocument doc;
    expect(parse_style("{\"version\":8,\"layers\":["
                       "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":"
                       "\"land\",\"paint\":{\"fill-color\":\"#f5f3e9\"}}"
                       "]}",
                       &doc),
           "aabb tile retained style");
    OGRPolygon poly;
    OGRLinearRing* ring = new OGRLinearRing();
    ring->addPoint(11, 1);
    ring->addPoint(13, 1);
    ring->addPoint(12, 3);
    ring->addPoint(11, 1);
    poly.addRingDirectly(ring);
    LayerBatch land;
    land.source_layer = "land";
    land.geoms = {&poly};
    LayoutInput in;
    in.view.width_px = 768;
    in.view.height_px = 256;
    in.view.min_x = 0;
    in.view.min_y = 0;
    in.view.max_x = 30;
    in.view.max_y = 10;
    in.style = &doc;
    in.zoom = 8;
    const vista::MapIR first = layout.build(in, {land});
    expect(!first.items.empty() && first.items[0].cache_key != 0,
           "tagged cache_key on valid view");
    struct MapSlices : vista::SliceCache {
      std::map<uint64_t, std::vector<vista::DrawItem>> store;
      const std::vector<vista::DrawItem>* find(uint64_t key) const override {
        const auto it = store.find(key);
        if (it == store.end() || it->second.empty()) {
          return nullptr;
        }
        return &it->second;
      }
    } slices;
    for (const auto& item : first.items) {
      if (item.cache_key != 0) {
        slices.store[item.cache_key].push_back(item);
      }
    }
    in.retained_slices = &slices;
    const double tile_w = vista::detail::layout_tile_world_size(in.view);
    in.view.min_x += tile_w;
    in.view.max_x += tile_w;
    const vista::MapIR hit = layout.build(in, {land});
    expect(!hit.items.empty() &&
               hit.items[0].cache_key == first.items[0].cache_key,
           "pan one tile column keeps unmoved cache_key");

    in.view.min_x += tile_w * 4.0;
    in.view.max_x += tile_w * 4.0;
    const vista::MapIR miss = layout.build(in, {land});
    expect(miss.items.empty() ||
               miss.items[0].cache_key != first.items[0].cache_key,
           "pan off the geom tile misses retained key");
    expect(vista::detail::layout_aabb_tile_id(vista::View{}) == 0,
           "untagged cache_key==0 for degenerate view");
  }

  // N2: pack_geoms drops envelopes outside the view (+outset).
  {
    gis::style::StyleDocument doc;
    expect(parse_style("{\"version\":8,\"layers\":["
                       "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":"
                       "\"land\",\"paint\":{\"fill-color\":\"#f5f3e9\"}}"
                       "]}",
                       &doc),
           "pack style");
    OGRPolygon far;
    OGRLinearRing* ring = new OGRLinearRing();
    ring->addPoint(1000, 1000);
    ring->addPoint(1002, 1000);
    ring->addPoint(1001, 1003);
    ring->addPoint(1000, 1000);
    far.addRingDirectly(ring);
    LayerBatch land;
    land.source_layer = "land";
    land.geoms = {&far};
    LayoutInput in;
    in.view = square_view(100, 10);
    in.style = &doc;
    in.zoom = 8;
    const vista::MapIR packed = layout.build(in, {land});
    expect(count_kind(packed, DrawKind::kFill) == 0, "pack drops off-view fill");
  }

  // M3: adjacent same-fill DrawItems coalesce; overlay vs world stay split.
  {
    gis::style::StyleDocument doc;
    expect(parse_style("{\"version\":8,\"layers\":["
                       "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":"
                       "\"land\",\"paint\":{\"fill-color\":\"#112233\"}}"
                       "]}",
                       &doc),
           "coalesce fill style");
    std::vector<OGRPolygon> polys(2);
    std::vector<OGRLinearRing> rings(2);
    LayerBatch land;
    land.source_layer = "land";
    for (size_t i = 0; i < polys.size(); ++i) {
      const double o = static_cast<double>(i) * 2.0;
      rings[i].addPoint(o, o);
      rings[i].addPoint(o + 1.0, o);
      rings[i].addPoint(o + 0.5, o + 1.0);
      rings[i].closeRings();
      polys[i].addRing(&rings[i]);
      land.geoms.push_back(&polys[i]);
    }
    LayoutInput in;
    in.view = square_view(200, 20);
    in.style = &doc;
    in.zoom = 8;
    const vista::MapIR frame = layout.build(in, {land});
    expect(count_kind(frame, DrawKind::kFill) == 1 &&
               !frame.items.empty() && frame.items[0].indices.size() >= 6,
           "coalesce reduces adjacent same-fill items");
  }

  // M4: live gen bumped → build returns without hanging (skips tess).
  {
    gis::style::StyleDocument doc;
    expect(parse_style("{\"version\":8,\"layers\":["
                       "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":"
                       "\"land\",\"paint\":{\"fill-color\":\"#f5f3e9\"}}"
                       "]}",
                       &doc),
           "abort style");
    OGRPolygon poly;
    OGRLinearRing* ring = new OGRLinearRing();
    ring->addPoint(1, 1);
    ring->addPoint(3, 1);
    ring->addPoint(2, 3);
    ring->addPoint(1, 1);
    poly.addRingDirectly(ring);
    LayerBatch land;
    land.source_layer = "land";
    land.geoms = {&poly};
    LayoutInput in;
    in.view = square_view(100, 10);
    in.style = &doc;
    in.zoom = 8;
    std::atomic<uint64_t> live{2};
    in.layout_gen = 1;
    in.live_layout_gen = &live;
    const vista::MapIR aborted = layout.build(in, {land});
    expect(aborted.items.empty(), "stale layout_gen skips emit");
    in.layout_gen = 0;
    const vista::MapIR never = layout.build(in, {land});
    expect(!never.items.empty(), "layout_gen 0 never aborts");
  }

  if (g_fails) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("map2d_test OK\n");
  return 0;
}
