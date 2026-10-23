// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "base/process/switches.h"
#include "vista/component/map/batch.h"
#include "vista/component/map/carto/filter.h"
#include "vista/component/map/carto/collision.h"
#include "vista/component/map/layout/policy/slice_key.h"
#include "vista/component/map/shade/multiply.h"
#include "vista/component/map/ir.h"
#include "gis/style/paint_resolve.h"
#include "gis/style/document/style_document.h"
#include "gis/style/eval/style_rules.h"
#include "ogrsf_frmts.h"

namespace {

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

using vista::Layout;

// Shared layout and fixed glyph advances for map frame assertions.
class Map2dFrame : public ::testing::Test {
 protected:
  Layout layout;
  FixedAdvance metrics;
};

using vista::BatchFeature;
using vista::BatchGeomKind;
using vista::BatchLayer;
using vista::DrawKind;
using vista::LayerBatch;
using vista::LayerBatchSet;
using vista::LayoutInput;
using vista::TileSlot;
using vista::build_layer_batches;

// AVX2 groups of 8 plus a scalar tail must match the per-pixel formula.
TEST_F(Map2dFrame, MultiplyMatchesScalar) {
  auto scalar_px = [](uint8_t* px, float op) {
    if (px[3] < 160) {
      px[0] = 0;
      px[1] = 0;
      px[2] = 0;
      px[3] = 0;
      return;
    }
    const float r = static_cast<float>(px[0]) / 255.f;
    const float g = static_cast<float>(px[1]) / 255.f;
    const float b = static_cast<float>(px[2]) / 255.f;
    const float luma = 0.299f * r + 0.587f * g + 0.114f * b;
    const float m = (1.f - op) + op * luma;
    const float clamped = (std::max)(0.f, (std::min)(1.f, m));
    const auto factor = static_cast<uint8_t>(clamped * 255.f + 0.5f);
    px[0] = factor;
    px[1] = factor;
    px[2] = factor;
    px[3] = 255;
  };
  constexpr float k_op = 0.55f;
  constexpr int k_pixels = 19;
  std::vector<uint8_t> got(static_cast<size_t>(k_pixels) * 4u);
  for (int i = 0; i < k_pixels; ++i) {
    got[static_cast<size_t>(i) * 4u + 0] = static_cast<uint8_t>(i * 13);
    got[static_cast<size_t>(i) * 4u + 1] = static_cast<uint8_t>(255 - i * 7);
    got[static_cast<size_t>(i) * 4u + 2] = static_cast<uint8_t>(i * 19);
    got[static_cast<size_t>(i) * 4u + 3] =
        (i % 5 == 0) ? static_cast<uint8_t>(100) : static_cast<uint8_t>(200);
  }
  std::vector<uint8_t> expected = got;
  for (int i = 0; i < k_pixels; ++i) {
    scalar_px(expected.data() + static_cast<size_t>(i) * 4u, k_op);
  }
  vista::apply_multiply_coverage(got, k_op);
  EXPECT_TRUE(got == expected) << "multiply avx2 matches scalar formula";
  std::fflush(stderr);
}

// Empty input still carries the default background and no meshes.
TEST_F(Map2dFrame, EmptyFrameKeepsOcean) {
  const vista::MapIR frame = layout.build({}, {});
  EXPECT_TRUE(frame.items.empty()) << "empty frame keeps background";
  EXPECT_TRUE(frame.background_rgba == 0xffaad3dfu) << "empty frame keeps ocean clear";
}

// Painter order: background is a clear; fill is emitted before line.
TEST_F(Map2dFrame, PainterOrderFillBeforeLine) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":["
             "{\"id\":\"bg\",\"type\":\"background\",\"paint\":{"
             "\"background-color\":\"#112233\",\"background-opacity\":0.8}},"
             "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":\"land\","
             "\"paint\":{\"fill-color\":\"#f5f3e9\",\"fill-opacity\":1}},"
             "{\"id\":\"road\",\"type\":\"line\",\"source-layer\":\"road\","
             "\"paint\":{\"line-color\":\"#ffffff\",\"line-width\":2}}"
             "]}",
             &doc)) << "painter order fill before line";
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
  EXPECT_TRUE(frame.background_rgba == 0xff112233u) << "painter background is not a mesh";
  EXPECT_TRUE(almost_eq(frame.background_opacity, 0.8f)) << "painter background is not a mesh";
  EXPECT_TRUE(frame.items.size() == 2) << "painter order fill before line";
  EXPECT_TRUE(frame.items.size() == 2 && frame.items[0].kind == DrawKind::kFill &&
             frame.items[1].kind == DrawKind::kLine) << "painter order fill before line";
  EXPECT_TRUE(frame.items.size() == 2 && !frame.items[0].pixel_space &&
             !frame.items[1].pixel_space) << "painter order fill before line";
}

// Adjacent land polygons take different colors of a four-coloring when opted in.
TEST_F(Map2dFrame, ProvinceFourColor) {
  base::set_switch("map2d-province-colors", "1");
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":[{"
             "\"id\":\"land\",\"type\":\"fill\",\"source-layer\":\"land\","
             "\"paint\":{\"fill-color\":\"#111111\",\"fill-opacity\":1}"
             "}]}",
             &doc)) << "province four-color style";
  OGRLinearRing left_ring;
  left_ring.addPoint(0, 0);
  left_ring.addPoint(2, 0);
  left_ring.addPoint(2, 2);
  left_ring.addPoint(0, 2);
  left_ring.closeRings();
  OGRLinearRing right_ring;
  right_ring.addPoint(2, 0);
  right_ring.addPoint(4, 0);
  right_ring.addPoint(4, 2);
  right_ring.addPoint(2, 2);
  right_ring.closeRings();
  OGRPolygon left_poly;
  OGRPolygon right_poly;
  left_poly.addRing(&left_ring);
  right_poly.addRing(&right_ring);
  LayerBatch land;
  land.source_layer = "land";
  land.geoms.push_back(&left_poly);
  land.geoms.push_back(&right_poly);
  LayoutInput in;
  in.view = square_view(100, 10);
  in.style = &doc;
  in.zoom = 8;
  const vista::MapIR frame = layout.build(in, {land});
  base::clear_switch("map2d-province-colors");
  EXPECT_TRUE(frame.items.size() == 2) << "province four-color emits both fills";
  EXPECT_TRUE(frame.items.size() == 2 && frame.items[0].kind == DrawKind::kFill &&
             frame.items[1].kind == DrawKind::kFill &&
             frame.items[0].rgba != frame.items[1].rgba &&
             frame.items[0].rgba != 0xFF111111u &&
             frame.items[1].rgba != 0xFF111111u) << "province four-color adjacent fills differ";
}

// Default: land keeps style fill-color (product carto cream), no province tint.
TEST_F(Map2dFrame, LandKeepsStyleFillWithoutProvinceSwitch) {
  base::clear_switch("map2d-province-colors");
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":[{"
             "\"id\":\"land\",\"type\":\"fill\",\"source-layer\":\"land\","
             "\"paint\":{\"fill-color\":\"#f5f3e9\",\"fill-opacity\":1}"
             "}]}",
             &doc)) << "cream land style";
  OGRLinearRing left_ring;
  left_ring.addPoint(0, 0);
  left_ring.addPoint(2, 0);
  left_ring.addPoint(2, 2);
  left_ring.addPoint(0, 2);
  left_ring.closeRings();
  OGRLinearRing right_ring;
  right_ring.addPoint(2, 0);
  right_ring.addPoint(4, 0);
  right_ring.addPoint(4, 2);
  right_ring.addPoint(2, 2);
  right_ring.closeRings();
  OGRPolygon left_poly;
  OGRPolygon right_poly;
  left_poly.addRing(&left_ring);
  right_poly.addRing(&right_ring);
  LayerBatch land;
  land.source_layer = "land";
  land.geoms.push_back(&left_poly);
  land.geoms.push_back(&right_poly);
  LayoutInput in;
  in.view = square_view(100, 10);
  in.style = &doc;
  in.zoom = 8;
  const vista::MapIR frame = layout.build(in, {land});
  // Same cream coalesces to one DrawItem; color must stay style #f5f3e9.
  EXPECT_TRUE(count_kind(frame, DrawKind::kFill) == 1 && !frame.items.empty() &&
             frame.items[0].kind == DrawKind::kFill &&
             frame.items[0].rgba == 0xFFf5f3e9u)
      << "default land keeps cream fill-color";
}

// Two labels on the same point: the higher priority (lower score) stays.
TEST_F(Map2dFrame, OverlapKeepsHigherPriority) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":[{"
             "\"id\":\"label\",\"type\":\"symbol\",\"source-layer\":\"label\","
             "\"layout\":{\"symbol-placement\":\"point\",\"text-field\":\"{name}\","
             "\"text-size\":16,\"text-anchor\":\"center\"}"
             "}]}",
             &doc)) << "overlapping labels drop the lower priority";
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
  EXPECT_TRUE(texts >= 7) << "overlapping labels keep higher priority";
  EXPECT_TRUE(!frame.items.empty() && frame.items[0].kind == DrawKind::kText &&
             frame.items[0].codepoint == static_cast<uint32_t>('C')) << "overlapping labels keep higher priority";
}

// text-color paint reaches glyph DrawItems (CJK along-line still places).
TEST_F(Map2dFrame, TextColorAlongLine) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":[{"
             "\"id\":\"city\",\"type\":\"symbol\",\"source-layer\":\"label\","
             "\"layout\":{\"symbol-placement\":\"line\",\"text-field\":\"{name}\","
             "\"text-size\":16,\"text-anchor\":\"center\"},"
             "\"paint\":{\"text-color\":\"#336699\"}"
             "}]}",
             &doc)) << "text-color along-line";
  OGRLineString line;
  line.addPoint(1, 5);
  line.addPoint(9, 5);
  LayerBatch labels;
  labels.source_layer = "label";
  labels.geoms.push_back(&line);
  gis::style::AttrMap attrs;
  // Beijing UTF-8 — product metrics path uses YaHei; tests use FixedAdvance.
  attrs["name"] = "\xe5\x8c\x97\xe4\xba\xac";
  attrs["class"] = "title";
  labels.attrs.push_back(attrs);
  LayoutInput in;
  in.view = square_view(200, 10);
  in.style = &doc;
  in.zoom = 10;
  in.metrics = &metrics;
  const vista::MapIR frame = layout.build(in, {labels});
  EXPECT_TRUE(count_kind(frame, DrawKind::kText) >= 1) << "text-color along-line";
  bool colored = false;
  for (const auto& item : frame.items) {
    if (item.kind == DrawKind::kText && item.rgba == 0xFF336699u) {
      colored = true;
    }
  }
  EXPECT_TRUE(colored) << "text-color along-line";
}

// Line placement takes the midpoint tangent in screen space (y down).
TEST_F(Map2dFrame, AlongLineAngle) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":[{"
             "\"id\":\"road-label\",\"type\":\"symbol\",\"source-layer\":\"road\","
             "\"layout\":{\"symbol-placement\":\"line\",\"text-field\":\"{name}\","
             "\"text-size\":14,\"text-anchor\":\"center\"}"
             "}]}",
             &doc)) << "along-line angle";
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
  EXPECT_TRUE(count_kind(frame, DrawKind::kText) == 4) << "along-line angle";
  bool angled = false;
  for (const auto& item : frame.items) {
    if (item.kind == DrawKind::kText && item.pixel_space &&
        almost_eq(item.angle_rad, -3.14159265f / 4.f)) {
      angled = true;
    }
  }
  EXPECT_TRUE(angled) << "along-line angle";
}

// Horizontal along-line labels stay near 0 rad (screen y down, upright).
TEST_F(Map2dFrame, AlongLineFlat) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":[{"
             "\"id\":\"road-label\",\"type\":\"symbol\",\"source-layer\":\"road\","
             "\"layout\":{\"symbol-placement\":\"line\",\"text-field\":\"{name}\","
             "\"text-size\":14,\"text-anchor\":\"center\"}"
             "}]}",
             &doc)) << "along-line angle sign";
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
  EXPECT_TRUE(flat) << "along-line angle sign";
}

// Collision helpers: path length, pose fractions, mixed-script estimate.
TEST_F(Map2dFrame, CollisionHelpers) {
  const int xy[] = {0, 0, 100, 0, 100, 50};
  EXPECT_TRUE(almost_eq(vista::detail::line_path_length_px(xy, 3), 150.f)) << "line_path_length_px";
  vista::detail::LineLabelPose mid;
  vista::detail::LineLabelPose q;
  EXPECT_TRUE(vista::detail::line_label_pose_at(xy, 3, 0.5f, &mid)) << "pose_at mid";
  EXPECT_TRUE(vista::detail::line_label_pose_at(xy, 3, 0.25f, &q)) << "pose_at quarter";
  EXPECT_TRUE(mid.x == 75 && mid.y == 0) << "pose_at mid xy";
  EXPECT_TRUE(q.x == 37 || q.x == 38) << "pose_at quarter x";
  EXPECT_TRUE(vista::detail::line_fits_label(100.f, 80.f)) << "line_fits ok";
  EXPECT_TRUE(!vista::detail::line_fits_label(80.f, 100.f)) << "line_fits reject";
  float slots[8];
  const int n = vista::detail::line_label_slot_fractions(8, slots);
  EXPECT_TRUE(n >= 5 && almost_eq(slots[0], 0.5f)) << "slot fractions mid first";
  // Latin narrower than CJK at same unit count ("AB" vs two CJK).
  const float latin =
      vista::detail::estimate_run_width_px("AB", 20.f);
  const char cjk[] = "\xe4\xb8\xad\xe5\x9b\xbd";  // 涓浗
  const float wide = vista::detail::estimate_run_width_px(cjk, 20.f);
  EXPECT_TRUE(wide > latin * 1.4f) << "CJK estimate wider than Latin";
  vista::detail::LabelBox upright;
  upright.left = 0;
  upright.top = 0;
  upright.right = 40;
  upright.bottom = 10;
  upright.priority = 1;
  const vista::detail::LabelBox rot =
      vista::detail::rotate_label_box(upright, 90.f);
  EXPECT_TRUE(rot.right - rot.left >= 10 && rot.bottom - rot.top >= 40) << "rotate_label_box aabb";
}

// Point blocker at line midpoint: road label still places via alternate slot.
TEST_F(Map2dFrame, AlongLineSlotRetry) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":["
             "{\"id\":\"blocker\",\"type\":\"symbol\",\"source-layer\":\"poi\","
             "\"layout\":{\"text-field\":\"{name}\",\"text-size\":20,"
             "\"text-anchor\":\"center\"}},"
             "{\"id\":\"road-label\",\"type\":\"symbol\",\"source-layer\":\"road\","
             "\"layout\":{\"symbol-placement\":\"line\",\"text-field\":\"{name}\","
             "\"text-size\":12,\"text-anchor\":\"center\"}}"
             "]}",
             &doc)) << "along-line slot retry";
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
  EXPECT_TRUE(texts >= 7) << "along-line slot retry";
  bool saw_offset = false;
  for (const auto& item : frame.items) {
    if (item.kind == DrawKind::kText && item.codepoint == 'R') {
      // Midpoint of 0..10 at zoom view is x=200; alternate slots leave mid.
      if (std::fabs(item.anchor_x - 200.f) > 20.f) {
        saw_offset = true;
      }
    }
  }
  EXPECT_TRUE(saw_offset) << "along-line slot offset from mid";
}

// Along-line text shorter than path width is dropped (icon-less).
TEST_F(Map2dFrame, ShortPathDropsLongLabel) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":[{"
             "\"id\":\"road-label\",\"type\":\"symbol\",\"source-layer\":\"road\","
             "\"layout\":{\"symbol-placement\":\"line\",\"text-field\":\"{name}\","
             "\"text-size\":40,\"text-anchor\":\"center\"}"
             "}]}",
             &doc)) << "short path drops long label";
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
  EXPECT_TRUE(count_kind(frame, DrawKind::kText) == 0) << "short path drops long label";
}

// Icon+text union: both kinds emit when a single packed box is kept.
TEST_F(Map2dFrame, IconTextPackedBox) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":[{"
             "\"id\":\"label\",\"type\":\"symbol\",\"source-layer\":\"label\","
             "\"layout\":{\"text-field\":\"{name}\",\"text-size\":14,"
             "\"text-anchor\":\"left\",\"icon-image\":\"pin\",\"icon-size\":1}"
             "}]}",
             &doc)) << "icon+text packed box";
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
  EXPECT_TRUE(count_kind(frame, DrawKind::kIcon) == 1 &&
             count_kind(frame, DrawKind::kText) == 1) << "icon+text packed box";
}

// Style text-halo-width flows to DrawItem; omitted width uses carto halo_px.
TEST_F(Map2dFrame, HaloWidthOnGlyphs) {
  gis::style::StyleDocument styled;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":[{"
             "\"id\":\"label\",\"type\":\"symbol\",\"source-layer\":\"label\","
             "\"layout\":{\"text-field\":\"{name}\",\"text-size\":16,"
             "\"text-anchor\":\"center\"},"
             "\"paint\":{\"text-halo-color\":\"#ffffff\",\"text-halo-width\":3}"
             "}]}",
             &styled)) << "style halo width on glyphs";
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
  EXPECT_TRUE(!styled_frame.items.empty() &&
             styled_frame.items[0].halo_width_px == 3.f &&
             styled_frame.items[0].halo_rgba == 0xffffffffu) << "style halo width on glyphs";

  gis::style::StyleDocument carto;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":[{"
             "\"id\":\"label\",\"type\":\"symbol\",\"source-layer\":\"label\","
             "\"layout\":{\"text-field\":\"{name}\",\"text-size\":16,"
             "\"text-anchor\":\"center\"}"
             "}]}",
             &carto)) << "carto halo when width omitted";
  in.style = &carto;
  const vista::MapIR carto_frame = layout.build(in, {labels});
  EXPECT_TRUE(!carto_frame.items.empty() &&
             carto_frame.items[0].halo_width_px ==
                 static_cast<float>(vista::detail::halo_px(0)) &&
             carto_frame.items[0].halo_rgba == 0xffffffffu) << "carto halo when width omitted";
}

// Collision grid drops a second overlapping label at the same anchor.
TEST_F(Map2dFrame, OverlapDropWithHalo) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":[{"
             "\"id\":\"label\",\"type\":\"symbol\",\"source-layer\":\"label\","
             "\"layout\":{\"text-field\":\"{name}\",\"text-size\":16,"
             "\"text-anchor\":\"center\"}"
             "}]}",
             &doc)) << "overlap drop with halo padding";
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
  EXPECT_TRUE(count_kind(frame, DrawKind::kText) >= 5) << "overlap drop with halo padding";
}

// Circle is a triangle fan, not the old four-vertex diamond.
TEST_F(Map2dFrame, CircleIsTriangleFan) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":[{"
             "\"id\":\"poi\",\"type\":\"circle\",\"source-layer\":\"poi\","
             "\"paint\":{\"circle-color\":\"#2244aa\",\"circle-radius\":8,"
             "\"circle-opacity\":0.9}"
             "}]}",
             &doc)) << "circle has more than 4 vertices";
  OGRPoint pt(5, 5);
  LayerBatch pois;
  pois.source_layer = "poi";
  pois.geoms.push_back(&pt);
  LayoutInput in;
  in.view = square_view(100, 10);
  in.style = &doc;
  in.zoom = 12;
  const vista::MapIR frame = layout.build(in, {pois});
  EXPECT_TRUE(frame.items.size() == 1 && frame.items[0].kind == DrawKind::kCircle &&
             frame.items[0].vertices.size() > 4 && !frame.items[0].pixel_space) << "circle has more than 4 vertices";
}

// line-width pixels become a world ribbon via world-units-per-pixel.
TEST_F(Map2dFrame, LineWidthWorldRibbon) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":[{"
             "\"id\":\"road\",\"type\":\"line\",\"source-layer\":\"road\","
             "\"paint\":{\"line-color\":\"#ffffff\",\"line-width\":4},"
             "\"layout\":{\"line-cap\":\"butt\",\"line-join\":\"miter\"}"
             "}]}",
             &doc)) << "line width passed through";
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
  EXPECT_TRUE(frame.items.size() == 1 && frame.items[0].kind == DrawKind::kLine) << "line width passed through";
  float max_dy = 0.f;
  if (!frame.items.empty()) {
    for (const auto& v : frame.items[0].vertices) {
      max_dy = (std::max)(max_dy, std::fabs(v.y - 50.f));
    }
  }
  EXPECT_TRUE(almost_eq(max_dy, 2.f)) << "line width passed through";
}

// Raster quad stays in lon/lat. texture_key rides in codepoint.
TEST_F(Map2dFrame, RasterQuadLonLat) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":[{"
             "\"id\":\"sat\",\"type\":\"raster\","
             "\"paint\":{\"raster-opacity\":0.5}"
             "}]}",
             &doc)) << "raster quad in lon/lat";
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
  EXPECT_TRUE(frame.items.size() == 1 && frame.items[0].kind == DrawKind::kRaster &&
             frame.items[0].vertices.size() == 4 &&
             frame.items[0].indices.size() == 6 &&
             frame.items[0].codepoint == 42 && frame.items[0].symbol_id.empty() &&
             !frame.items[0].pixel_space &&
             frame.items[0].blend == vista::DrawBlend::kOver) << "raster quad in lon/lat";
  if (frame.items.size() == 1) {
    const auto& v = frame.items[0].vertices;
    EXPECT_TRUE(almost_eq(v[0].x, 1.f) && almost_eq(v[0].y, 4.f) && almost_eq(v[0].u, 0.f) &&
               almost_eq(v[0].v, 0.f)) << "raster quad in lon/lat";
    EXPECT_TRUE(almost_eq(v[2].x, 3.f) && almost_eq(v[2].y, 2.f) && almost_eq(v[2].u, 1.f) &&
               almost_eq(v[2].v, 1.f)) << "raster quad in lon/lat";
  }
}

// Missing icon still places text. Missing metrics skips text and keeps the icon.
TEST_F(Map2dFrame, MissingIconOrMetrics) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":[{"
             "\"id\":\"label\",\"type\":\"symbol\",\"source-layer\":\"label\","
             "\"layout\":{\"text-field\":\"{name}\",\"text-size\":16,"
             "\"icon-image\":\"pin\",\"icon-size\":1}"
             "}]}",
             &doc)) << "missing icon still places text";
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
  EXPECT_TRUE(count_kind(text_only, DrawKind::kIcon) == 0 &&
             count_kind(text_only, DrawKind::kText) == 2) << "missing icon still places text";
  in.metrics = nullptr;
  in.symbols.push_back(vista::SymbolAsset{"pin", 16.f, 16.f});
  const vista::MapIR icon_only = layout.build(in, {labels});
  EXPECT_TRUE(count_kind(icon_only, DrawKind::kText) == 0 &&
             count_kind(icon_only, DrawKind::kIcon) == 1 &&
             icon_only.items[0].symbol_id == "pin") << "missing metrics skips text";
}

// Parallel tess path (鈮? geoms / flattened MultiLineString parts) keeps counts.
TEST_F(Map2dFrame, ParallelTessKeepsCounts) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":["
             "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":\"land\","
             "\"paint\":{\"fill-color\":\"#f5f3e9\",\"fill-opacity\":1}},"
             "{\"id\":\"road\",\"type\":\"line\",\"source-layer\":\"road\","
             "\"paint\":{\"line-color\":\"#ffffff\",\"line-width\":2}}"
             "]}",
             &doc)) << "parallel tess keeps draw counts";
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
  EXPECT_TRUE(count_kind(frame, DrawKind::kFill) == 1) << "parallel fill coalesces same-style polygons";
  EXPECT_TRUE(count_kind(frame, DrawKind::kLine) == 1) << "parallel line coalesces MultiLineString parts";
}

// Hillshade layer emits a raster underlay when host supplies tiles.
TEST_F(Map2dFrame, HillshadeRasterUnderlay) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style("{\"version\":8,\"layers\":["
                     "{\"id\":\"shade\",\"type\":\"hillshade\",\"paint\":{}},"
                     "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":"
                     "\"land\",\"paint\":{\"fill-color\":\"#f5f3e9\"}}"
                     "]}",
                     &doc)) << "hillshade style parses";
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
  EXPECT_TRUE(!frame.items.empty() && frame.items[0].kind == DrawKind::kRaster &&
             frame.items[0].codepoint == 0x48534844u &&
             frame.items[0].blend == vista::DrawBlend::kOver) << "hillshade underlay first";
  EXPECT_TRUE(count_kind(frame, DrawKind::kFill) == 1) << "area fill over bake underlay";
  EXPECT_TRUE(frame.items.size() >= 2 &&
             frame.items[1].kind == DrawKind::kFill &&
             frame.items[1].opacity <= 0.55f + 1e-3f) << "area translucent over bake";
}

// Points and labels stay above lines even when the style lists them first.
TEST_F(Map2dFrame, PointsAboveLines) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style(
             "{"
             "\"version\":8,\"layers\":["
             "{\"id\":\"label\",\"type\":\"symbol\",\"source-layer\":\"poi\","
             "\"layout\":{\"text-field\":\"{name}\",\"text-size\":12}},"
             "{\"id\":\"poi\",\"type\":\"circle\",\"source-layer\":\"poi\","
             "\"paint\":{\"circle-color\":\"#e65100\",\"circle-radius\":3}},"
             "{\"id\":\"road\",\"type\":\"line\",\"source-layer\":\"road\","
             "\"paint\":{\"line-color\":\"#ffffff\",\"line-width\":1}}"
             "]}",
             &doc)) << "band order style parses";
  OGRPoint pt(5, 5);
  LayerBatch pois;
  pois.source_layer = "poi";
  pois.geoms.push_back(&pt);
  gis::style::AttrMap attrs;
  attrs["name"] = "A";
  pois.attrs.push_back(attrs);
  OGRLineString road;
  road.addPoint(0, 0);
  road.addPoint(10, 0);
  LayerBatch roads;
  roads.source_layer = "road";
  roads.geoms.push_back(&road);
  LayoutInput in;
  in.view = square_view(800, 10);
  in.style = &doc;
  in.zoom = 10;
  in.metrics = &metrics;
  const vista::MapIR frame = layout.build(in, {pois, roads});
  int line_i = -1;
  int circle_i = -1;
  int text_i = -1;
  for (int i = 0; i < static_cast<int>(frame.items.size()); ++i) {
    if (line_i < 0 && frame.items[static_cast<size_t>(i)].kind == DrawKind::kLine) {
      line_i = i;
    }
    if (circle_i < 0 &&
        frame.items[static_cast<size_t>(i)].kind == DrawKind::kCircle) {
      circle_i = i;
    }
    if (text_i < 0 && frame.items[static_cast<size_t>(i)].kind == DrawKind::kText) {
      text_i = i;
    }
  }
  EXPECT_TRUE(line_i >= 0 && circle_i > line_i && text_i > circle_i) << "lines then points then labels";
}

// Jet bake underlay then translucent land so both remain readable.
TEST_F(Map2dFrame, JetHillshadeUnderFill) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style("{\"version\":8,\"layers\":["
                     "{\"id\":\"shade\",\"type\":\"hillshade\",\"paint\":{"
                     "\"hillshade-color-ramp\":\"jet\"}},"
                     "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":"
                     "\"land\",\"paint\":{\"fill-color\":\"#f5f3e9\"}}"
                     "]}",
                     &doc)) << "jet hillshade style parses";
  vista::TileSlot hs;
  hs.min_x = 0;
  hs.min_y = 0;
  hs.max_x = 20;
  hs.max_y = 20;
  hs.opacity = 1.f;
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
  EXPECT_TRUE(!frame.items.empty() && frame.items[0].kind == DrawKind::kRaster &&
             frame.items[0].blend == vista::DrawBlend::kOver) << "jet bake underlay first";
  EXPECT_TRUE(count_kind(frame, DrawKind::kFill) == 1) << "jet bake keeps translucent area";
  EXPECT_TRUE(!frame.items.empty() && frame.items[0].vertices.size() == 4 &&
             frame.items[0].vertices[0].y >
                 frame.items[0].vertices[2].y &&
             frame.items[0].vertices[0].v == 0.f &&
             frame.items[0].vertices[2].v == 1.f) << "hillshade v=0 at north (FlyCube/export AABB)";
}

// Jet tile that misses the view must not suppress land fill (otherwise the map becomes ocean-only when emit_hillshade drops the sheet).
TEST_F(Map2dFrame, JetMissKeepsLandFill) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style("{\"version\":8,\"layers\":["
                     "{\"id\":\"shade\",\"type\":\"hillshade\",\"paint\":{"
                     "\"hillshade-color-ramp\":\"jet\"}},"
                     "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":"
                     "\"land\",\"paint\":{\"fill-color\":\"#f5f3e9\"}}"
                     "]}",
                     &doc)) << "jet miss-view style parses";
  vista::TileSlot hs;
  hs.min_x = 100;
  hs.min_y = 100;
  hs.max_x = 120;
  hs.max_y = 120;
  hs.opacity = 1.f;
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
  EXPECT_TRUE(count_kind(frame, DrawKind::kRaster) == 0) << "jet tile outside view emits no raster";
  EXPECT_TRUE(count_kind(frame, DrawKind::kFill) >= 1) << "jet miss-view keeps cream land fill";
}

// Land that sits outside the DEM slot is clipped (no cream fringe).
TEST_F(Map2dFrame, LandClippedToDemSlot) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style("{\"version\":8,\"layers\":["
                     "{\"id\":\"shade\",\"type\":\"hillshade\",\"paint\":{}},"
                     "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":"
                     "\"land\",\"paint\":{\"fill-color\":\"#f5f3e9\"}}"
                     "]}",
                     &doc)) << "hillshade land-clip style";
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
  EXPECT_TRUE(count_kind(frame, DrawKind::kFill) == 0) << "land outside DEM slot dropped";
}

// dem_clip alone (hillshade DrawItem deferred) still drops cream fringe.
TEST_F(Map2dFrame, DemClipDropsCreamFringe) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style("{\"version\":8,\"layers\":["
                     "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":"
                     "\"land\",\"paint\":{\"fill-color\":\"#f5f3e9\"}}"
                     "]}",
                     &doc)) << "dem_clip land style";
  LayoutInput in;
  in.view = square_view(200, 20);
  in.style = &doc;
  in.zoom = 8;
  in.have_dem_clip = true;
  in.dem_clip.min_x = 0;
  in.dem_clip.min_y = 0;
  in.dem_clip.max_x = 5;
  in.dem_clip.max_y = 5;
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
  EXPECT_TRUE(count_kind(frame, DrawKind::kFill) == 0) << "dem_clip drops land outside DEM";
}

// Jet style + dem_clip without a raster tile must keep cream land — otherwise GPU present falls through to clear + lines (black China) when bake/upload lags the first layout.
TEST_F(Map2dFrame, JetDemClipKeepsLand) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style("{\"version\":8,\"layers\":["
                     "{\"id\":\"shade\",\"type\":\"hillshade\",\"paint\":{"
                     "\"hillshade-color-ramp\":\"jet\"}},"
                     "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":"
                     "\"land\",\"paint\":{\"fill-color\":\"#f5f3e9\"}}"
                     "]}",
                     &doc)) << "jet dem_clip cream style";
  LayoutInput in;
  in.view = square_view(200, 20);
  in.style = &doc;
  in.zoom = 8;
  in.have_dem_clip = true;
  in.dem_clip.min_x = 0;
  in.dem_clip.min_y = 0;
  in.dem_clip.max_x = 20;
  in.dem_clip.max_y = 20;
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
  EXPECT_TRUE(count_kind(frame, DrawKind::kFill) >= 1) << "jet dem_clip without tiles keeps cream land";
  EXPECT_TRUE(count_kind(frame, DrawKind::kRaster) == 0) << "jet dem_clip without tiles emits no raster";
}

// Fill-extrusion v1: prism walls + roof DrawItems for a zoom-matched layer.
TEST_F(Map2dFrame, FillExtrusionPrism) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style("{\"version\":8,\"layers\":["
                     "{\"id\":\"bldg\",\"type\":\"fill-extrusion\","
                     "\"source-layer\":\"bldg\",\"paint\":{"
                     "\"fill-extrusion-height\":2,"
                     "\"fill-extrusion-base\":0,"
                     "\"fill-extrusion-color\":\"#6688aa\","
                     "\"fill-extrusion-opacity\":1}}]}",
                     &doc)) << "fill-extrusion style parses";
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
  EXPECT_TRUE(count_kind(frame, DrawKind::kFill) >= 2) << "extrusion emits walls and roof";
  bool saw_lifted = false;
  for (const auto& item : frame.items) {
    for (const auto& v : item.vertices) {
      if (v.z > 1.5f) {
        saw_lifted = true;
      }
    }
  }
  EXPECT_TRUE(saw_lifted) << "extrusion roof has height z";
}

// Heatmap density: Style → RGBA8 + kRaster ('HEAT') (zoom-matched).
TEST_F(Map2dFrame, HeatmapRaster) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style("{\"version\":8,\"layers\":["
                     "{\"id\":\"heat\",\"type\":\"heatmap\","
                     "\"source-layer\":\"heatmap\",\"minzoom\":5,"
                     "\"paint\":{"
                     "\"heatmap-radius\":10,"
                     "\"heatmap-weight\":1,"
                     "\"heatmap-intensity\":1,"
                     "\"heatmap-color\":\"#ff6400\","
                     "\"heatmap-opacity\":0.5}}]}",
                     &doc)) << "heatmap style parses";
  LayoutInput in;
  in.view = square_view(100, 10);
  in.style = &doc;
  in.zoom = 8;
  vista::HeatmapBake bake;
  vista::layout_bind_heatmap_bake(&bake);
  OGRPoint a(3, 3);
  OGRPoint b(7, 7);
  LayerBatch batch;
  batch.source_layer = "heatmap";
  batch.geoms = {&a, &b};
  const vista::MapIR frame = layout.build(in, {batch});
  vista::layout_bind_heatmap_bake(nullptr);
  EXPECT_TRUE(count_kind(frame, DrawKind::kRaster) == 1) << "heatmap density emits kRaster";
  EXPECT_TRUE(count_kind(frame, DrawKind::kCircle) == 0) << "heatmap density is not circle splat";
  EXPECT_TRUE(bake.width > 0 && bake.height > 0 && !bake.rgba.empty() &&
             bake.rgba.size() == static_cast<size_t>(bake.width) *
                                     static_cast<size_t>(bake.height) * 4u) << "heatmap density bake rgba";
  EXPECT_TRUE(!frame.items.empty() &&
             frame.items[0].kind == DrawKind::kRaster &&
             frame.items[0].codepoint == vista::kHeatmapTextureKey &&
             frame.items[0].vertices.size() == 4) << "heatmap density raster key and quad";

  in.zoom = 3;
  bake = {};
  const vista::MapIR miss = layout.build(in, {batch});
  EXPECT_TRUE(miss.items.empty()) << "heatmap respects minzoom";
}

// Embedded cartography document parses and keeps casing before fill.
TEST_F(Map2dFrame, CartoCasingBeforeFill) {
  gis::style::StyleDocument doc;
  const std::string json = vista::default_carto_style_json();
  EXPECT_TRUE(parse_style(json, &doc)) << "default style JSON parses";
  EXPECT_TRUE(doc.layers.size() == 19) << "default style layer count";
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
  EXPECT_TRUE(casing >= 0 && fill > casing) << "casing before road fill";
  EXPECT_TRUE(hillshade > 0 && hillshade < casing) << "hillshade before roads";
  EXPECT_TRUE(heatmap > hillshade && heatmap < casing) << "heatmap after hillshade before roads";
  EXPECT_TRUE(doc.layers[static_cast<size_t>(hillshade)].type ==
             gis::style::LayerType::kHillshade) << "hillshade layer type";
  EXPECT_TRUE(doc.layers[static_cast<size_t>(heatmap)].type ==
             gis::style::LayerType::kHeatmap) << "heatmap layer type";
  // Product zoom_from_scale puts china overview near ~11; road minzoom 5
  // matches MapLibre national band so dual-stroke casing paints.
  EXPECT_TRUE(gis::style::layer_matches_zoom(doc.layers[static_cast<size_t>(casing)],
                                         8.0)) << "road-casing visible at national-equivalent zoom";
  EXPECT_TRUE(gis::style::layer_matches_zoom(doc.layers[static_cast<size_t>(fill)],
                                         8.0)) << "road fill visible at national-equivalent zoom";
  EXPECT_TRUE(gis::style::layer_matches_zoom(doc.layers[static_cast<size_t>(casing)],
                                        12.0)) << "road-casing matches regional zoom";
  EXPECT_TRUE(gis::style::layer_matches_zoom(doc.layers[static_cast<size_t>(fill)],
                                        12.0)) << "road fill matches regional zoom";
  {
    // line-width is a zoom expression — resolve, do not stof the JSON.
    gis::style::ResolvedPaint casing_paint;
    gis::style::ResolvedPaint fill_paint;
    gis::style::fill_resolved_paint(doc.layers[static_cast<size_t>(casing)],
                                    nullptr, {}, 11, &casing_paint);
    gis::style::fill_resolved_paint(doc.layers[static_cast<size_t>(fill)],
                                    nullptr, {}, 11, &fill_paint);
    EXPECT_TRUE(casing_paint.line_width > fill_paint.line_width) << "casing wider than fill";
    gis::style::AttrMap motorway;
    motorway["class"] = "motorway";
    gis::style::fill_resolved_paint(doc.layers[static_cast<size_t>(fill)],
                                    nullptr, motorway, 11, &fill_paint);
    EXPECT_TRUE(fill_paint.line_color == 0xffe10600u) << "motorway fill is red";
    gis::style::AttrMap trunk;
    trunk["class"] = "trunk";
    gis::style::fill_resolved_paint(doc.layers[static_cast<size_t>(fill)],
                                    nullptr, trunk, 11, &fill_paint);
    EXPECT_TRUE(fill_paint.line_color == 0xfff0b429u) << "trunk fill is amber";
    gis::style::AttrMap primary;
    primary["class"] = "primary";
    gis::style::fill_resolved_paint(doc.layers[static_cast<size_t>(fill)],
                                    nullptr, primary, 11, &fill_paint);
    EXPECT_TRUE(fill_paint.line_color == 0xff2e7d32u) << "primary fill is green";
    int river = -1;
    int stream_layer = -1;
    for (int i = 0; i < static_cast<int>(doc.layers.size()); ++i) {
      if (doc.layers[static_cast<size_t>(i)].id == "river") {
        river = i;
      }
      if (doc.layers[static_cast<size_t>(i)].id == "river-stream") {
        stream_layer = i;
      }
    }
    EXPECT_TRUE(river > fill) << "rivers paint above roads";
    EXPECT_TRUE(stream_layer > river) << "streams paint above main rivers";
    if (stream_layer >= 0) {
      EXPECT_TRUE(!gis::style::layer_matches_zoom(
                 doc.layers[static_cast<size_t>(stream_layer)], 12.0)) << "streams hidden on the national frame";
      EXPECT_TRUE(gis::style::layer_matches_zoom(
                 doc.layers[static_cast<size_t>(stream_layer)], 14.0)) << "streams visible when zoomed in";
    }
    gis::style::ResolvedPaint river_paint;
    gis::style::AttrMap main_river;
    main_river["class"] = "river";
    gis::style::fill_resolved_paint(doc.layers[static_cast<size_t>(river)],
                                    nullptr, main_river, 11, &river_paint);
    EXPECT_TRUE(river_paint.line_color == 0xff01579bu) << "main river is deep blue";
    gis::style::AttrMap stream;
    stream["class"] = "stream";
    gis::style::ResolvedPaint stream_paint;
    if (stream_layer >= 0) {
      gis::style::fill_resolved_paint(
          doc.layers[static_cast<size_t>(stream_layer)], nullptr, stream, 14,
          &stream_paint);
    }
    EXPECT_TRUE(stream_paint.line_color == 0xff039be5u) << "stream is lighter blue";
    EXPECT_TRUE(river_paint.line_width > stream_paint.line_width) << "main river wider than stream";
  }
  int river_label = -1;
  int road_label = -1;
  int place_label = -1;
  for (int i = 0; i < static_cast<int>(doc.layers.size()); ++i) {
    const std::string& id = doc.layers[static_cast<size_t>(i)].id;
    if (id == "river-label") {
      river_label = i;
    } else if (id == "road-label") {
      road_label = i;
    } else if (id == "label") {
      place_label = i;
    }
  }
  EXPECT_TRUE(river_label > 0 && road_label > 0 && place_label > 0) << "label layers present";
  if (river_label < 0 || road_label < 0 || place_label < 0) {
    return;
  }
  EXPECT_TRUE(doc.layers[0].type == gis::style::LayerType::kBackground &&
             doc.layers[1].type == gis::style::LayerType::kFill &&
             doc.layers[5].type == gis::style::LayerType::kLine &&
             river_label > 0 &&
             doc.layers[static_cast<size_t>(river_label)]
                     .layout.at("symbol-placement") == "line" &&
             road_label > river_label &&
             doc.layers[static_cast<size_t>(road_label)]
                     .layout.at("symbol-placement") == "line") << "default style layer order";
  gis::style::ResolvedPaint paint;
  gis::style::fill_resolved_paint(
      doc.layers[static_cast<size_t>(road_label)], nullptr, {}, 12, &paint);
  EXPECT_TRUE(paint.symbol_placement == "line") << "road-label placement";
  EXPECT_TRUE(almost_eq(paint.text_halo_width, 1.f)) << "road-label halo width";
  // Cream halo (#fff8e8) over parchment land / jet ridges.
  EXPECT_TRUE(paint.text_halo_color == 0xfffff8e8u) << "road-label halo color";
  gis::style::ResolvedPaint point_paint;
  gis::style::fill_resolved_paint(
      doc.layers[static_cast<size_t>(place_label)], nullptr, {}, 10,
      &point_paint);
  EXPECT_TRUE(almost_eq(point_paint.text_halo_width, 1.25f)) << "label text-halo-width";
  gis::style::ResolvedPaint heat_paint;
  gis::style::fill_resolved_paint(doc.layers[static_cast<size_t>(heatmap)],
                                  nullptr, {}, 10, &heat_paint);
  EXPECT_TRUE(heat_paint.type == gis::style::LayerType::kHeatmap &&
             almost_eq(heat_paint.heatmap_radius, 28.f)) << "default heatmap paint";
}

// Label importance, LOD, draw budget, and point-label nudges by scale.
TEST_F(Map2dFrame, LabelLodAndNudges) {
  using vista::detail::PointNudge;
  // zoom=8+log2(scale): 6→~10.6 省, 12→~11.6 市, 64→14 县, 256→16 更细.
  EXPECT_TRUE(vista::detail::label_min_importance(6.0) == 3) << "country scale floor";
  EXPECT_TRUE(vista::detail::label_min_importance(12.0) == 2) << "city scale floor";
  EXPECT_TRUE(vista::detail::label_min_importance(64.0) == 1) << "county scale floor";
  EXPECT_TRUE(vista::detail::label_min_importance(256.0) == 0) << "close scale floor";
  EXPECT_TRUE(vista::detail::lod_max_priority(6.f) == 1) << "lod province";
  EXPECT_TRUE(vista::detail::lod_max_priority(12.f) == 4) << "lod city";
  EXPECT_TRUE(vista::detail::lod_max_priority(64.f) == 6) << "lod county";
  EXPECT_TRUE(vista::detail::label_draw_budget(6.0) == 48) << "budget country";
  EXPECT_TRUE(vista::detail::label_draw_budget(12.0) == 160) << "budget city";
  EXPECT_TRUE(vista::detail::label_draw_budget(64.0) == 240) << "budget county";
  EXPECT_TRUE(vista::detail::label_draw_budget(256.0) == 320) << "budget close";
  PointNudge nudges[8];
  const int n = vista::detail::point_label_nudges(18, nudges, 8);
  EXPECT_TRUE(n == 8 && nudges[0].dx == 0 && nudges[0].dy == 0) << "nudge anchor";
  EXPECT_TRUE(nudges[1].dx == 0 && nudges[1].dy == -(18 + 4)) << "nudge up";
  EXPECT_TRUE(nudges[2].dx == 14 && nudges[2].dy == 0) << "nudge right";
  EXPECT_TRUE(nudges[3].dx == -14 && nudges[3].dy == 0) << "nudge left";
  EXPECT_TRUE(nudges[7].dx == 0 && nudges[7].dy == 18 + 2) << "nudge down";
}

// N1: cache_key is layer × world tile. Pan one column keeps unmoved tiles.
TEST_F(Map2dFrame, CacheKeyPanKeepsTiles) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style("{\"version\":8,\"layers\":["
                     "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":"
                     "\"land\",\"paint\":{\"fill-color\":\"#f5f3e9\"}}"
                     "]}",
                     &doc)) << "aabb tile retained style";
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
  // Empty SliceCache forces the warm tiled emit path (tags cache_key).
  // Cold cross-layer emit intentionally skips tagging for china first paint.
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
  in.retained_slices = &slices;
  const vista::MapIR first = layout.build(in, {land});
  EXPECT_TRUE(!first.items.empty() && first.items[0].cache_key != 0) << "tagged cache_key on valid view";
  for (const auto& item : first.items) {
    if (item.cache_key != 0) {
      slices.store[item.cache_key].push_back(item);
    }
  }
  const double tile_w = vista::detail::layout_tile_world_size(in.view);
  in.view.min_x += tile_w;
  in.view.max_x += tile_w;
  const vista::MapIR hit = layout.build(in, {land});
  EXPECT_TRUE(!hit.items.empty() &&
             hit.items[0].cache_key == first.items[0].cache_key) << "pan one tile column keeps unmoved cache_key";

  in.view.min_x += tile_w * 4.0;
  in.view.max_x += tile_w * 4.0;
  const vista::MapIR miss = layout.build(in, {land});
  EXPECT_TRUE(miss.items.empty() ||
             miss.items[0].cache_key != first.items[0].cache_key) << "pan off the geom tile misses retained key";
  EXPECT_TRUE(vista::detail::layout_aabb_tile_id(vista::View{}) == 0) << "untagged cache_key==0 for degenerate view";
}

// N2: pack_geoms drops envelopes outside the view (+outset).
TEST_F(Map2dFrame, PackGeomsDropsOutside) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style("{\"version\":8,\"layers\":["
                     "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":"
                     "\"land\",\"paint\":{\"fill-color\":\"#f5f3e9\"}}"
                     "]}",
                     &doc)) << "pack style";
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
  EXPECT_TRUE(count_kind(packed, DrawKind::kFill) == 0) << "pack drops off-view fill";
}

// M3: adjacent same-fill DrawItems coalesce; overlay vs world stay split.
TEST_F(Map2dFrame, CoalesceSameFill) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style("{\"version\":8,\"layers\":["
                     "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":"
                     "\"land\",\"paint\":{\"fill-color\":\"#112233\"}}"
                     "]}",
                     &doc)) << "coalesce fill style";
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
  EXPECT_TRUE(count_kind(frame, DrawKind::kFill) == 1 &&
             !frame.items.empty() && frame.items[0].indices.size() >= 6) << "coalesce reduces adjacent same-fill items";
}

// M4: live gen bumped → build returns without hanging (skips tess).
TEST_F(Map2dFrame, LiveGenSkipsTess) {
  gis::style::StyleDocument doc;
  EXPECT_TRUE(parse_style("{\"version\":8,\"layers\":["
                     "{\"id\":\"land\",\"type\":\"fill\",\"source-layer\":"
                     "\"land\",\"paint\":{\"fill-color\":\"#f5f3e9\"}}"
                     "]}",
                     &doc)) << "abort style";
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
  EXPECT_TRUE(aborted.items.empty()) << "stale layout_gen skips emit";
  in.layout_gen = 0;
  const vista::MapIR never = layout.build(in, {land});
  EXPECT_TRUE(!never.items.empty()) << "layout_gen 0 never aborts";
}

// National overview (~zoom 11, scale=8): land rings synth admin edges. Far overview (zoom<8) still skips synth for cold-batch cost.
TEST_F(Map2dFrame, NationalOverviewSynthsAdmin) {
  BatchLayer area;
  area.id = "china.area";
  area.name = "area";
  area.visible = true;
  BatchFeature poly;
  poly.kind = BatchGeomKind::kPolygon;
  poly.points = {{100.0, 30.0}, {110.0, 30.0}, {110.0, 40.0},
                 {100.0, 40.0}, {100.0, 30.0}};
  poly.fields = {{"kind", "land"}};
  area.features = {std::move(poly)};
  const LayerBatchSet national =
      build_layer_batches({area}, /*use_carto_slots=*/true, /*scale=*/8.0);
  size_t admin_n = 0;
  for (const LayerBatch& b : national.batches) {
    if (b.source_layer == "admin") {
      admin_n += b.geoms.size();
    }
  }
  EXPECT_TRUE(admin_n > 0) << "national overview synths land→admin edges";
  const LayerBatchSet far =
      build_layer_batches({area}, /*use_carto_slots=*/true, /*scale=*/0.5);
  size_t far_admin = 0;
  for (const LayerBatch& b : far.batches) {
    if (b.source_layer == "admin") {
      far_admin += b.geoms.size();
    }
  }
  EXPECT_TRUE(far_admin == 0) << "far overview skips land→admin synth";
}

// china_city lines: kind="line" + line_kind/class → carto road/river slots.
TEST_F(Map2dFrame, ChinaLineKindSlots) {
  BatchLayer lines;
  lines.id = "china.line";
  lines.name = "line";
  lines.visible = true;
  BatchFeature road;
  road.kind = BatchGeomKind::kLine;
  road.points = {{100.0, 30.0}, {101.0, 30.0}, {102.0, 31.0}};
  road.fields = {{"kind", "line"},
                 {"line_kind", "road"},
                 {"class", "motorway"}};
  BatchFeature river;
  river.kind = BatchGeomKind::kLine;
  river.points = {{110.0, 25.0}, {111.0, 25.5}, {112.0, 26.0}};
  river.fields = {{"kind", "line"}, {"line_kind", "river"}};
  BatchFeature ghost;
  ghost.kind = BatchGeomKind::kLine;
  ghost.points = {{120.0, 40.0}, {121.0, 40.0}};
  ghost.fields = {{"kind", "line"}};
  lines.features = {std::move(road), std::move(river), std::move(ghost)};
  // scale=0 keeps all probes that have a carto source (skip length gates).
  const LayerBatchSet batched =
      build_layer_batches({lines}, /*use_carto_slots=*/true, 0.0);
  bool saw_road = false;
  bool saw_river = false;
  bool saw_empty = false;
  for (const LayerBatch& batch : batched.batches) {
    if (batch.source_layer == "road" && !batch.geoms.empty()) {
      saw_road = true;
    } else if (batch.source_layer == "river" && !batch.geoms.empty()) {
      saw_river = true;
    } else if (batch.source_layer.empty() && !batch.geoms.empty()) {
      saw_empty = true;
    }
  }
  EXPECT_TRUE(saw_road) << "china line_kind=road maps to carto road";
  EXPECT_TRUE(saw_river) << "china line_kind=river maps to carto river";
  EXPECT_TRUE(!saw_empty) << "generic unlabeled line stays dropped";
}

}  // namespace

