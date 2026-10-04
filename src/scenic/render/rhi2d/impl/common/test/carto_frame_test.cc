// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/paint/carto/frame/carto_frame.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  // Names are ASCII here: legacy configs force execution-charset .936, so
  // UTF-8 source literals are unsafe. Priority is driven by adcode / kind.
  expect(scenic::detail::carto2d_label_priority("Beijing", "area", "", "110000") == 1,
         "provincial adcode priority");
  expect(scenic::detail::carto2d_label_priority("Nanjing", "city", "", "320100") == 2,
         "prefecture adcode priority");
  expect(scenic::detail::carto2d_label_priority("Guoluo", "area", "", "632600") == 2,
         "Qinghai prefecture adcode");
  expect(scenic::detail::carto2d_label_priority("County", "area", "", "320102") > 3,
         "county lower than prefecture");
  expect(scenic::detail::carto2d_label_priority("Xinjiang", "region", "", "") == 1,
         "region kind");

  expect(scenic::detail::carto2d_lod_max_priority(8.f) == 1,
         "country-scale LOD keeps province");
  expect(scenic::detail::carto2d_lod_max_priority(22.f) == 2,
         "wide country view allows prefecture");
  expect(scenic::detail::carto2d_lod_max_priority(80.f) >= 5, "street scale keeps all");

  expect(scenic::detail::carto2d_point_min_distance(8.f) >= 16,
         "country-scale point spacing");
  expect(scenic::detail::carto2d_point_min_distance(80.f) <= 4,
         "zoomed-in points denser");

  expect(scenic::detail::carto2d_label_px(2, 22.f) >= 15, "prefecture labels larger");
  expect(scenic::detail::carto2d_label_px(2, 22.f) > scenic::detail::carto2d_label_px(6, 22.f),
         "prefecture larger than county");
  expect(scenic::detail::carto2d_halo_px(1) >= 3, "strong halo on admin labels");
  expect(scenic::detail::carto2d_point_radius(22.f) <= 2, "thin POI at regional scale");
  expect(scenic::detail::carto2d_point_radius(8.f) <= 1, "thin POI at country scale");
  expect(scenic::detail::carto2d_stroke_px(22.f, true) >=
             scenic::detail::carto2d_stroke_px(22.f, false),
         "rivers at least as wide as admin");
  expect(scenic::detail::carto2d_is_river_kind("river"), "river kind");
  expect(scenic::detail::carto2d_is_road_kind("road"), "road kind");
  expect(!scenic::detail::carto2d_is_road_kind("area"), "area is not a road");
  expect(scenic::detail::carto2d_is_road_kind("line", "highway"),
         "road class from cls when kind generic");
  expect(scenic::detail::carto2d_road_class("expressway", "") == 3, "expressway class");
  expect(scenic::detail::carto2d_road_class("area", "road") == 1, "road from cls");
  {
    const float fblc = 40.f;
    const int w_road = scenic::detail::carto2d_road_width_px(fblc, 1);
    const int w_hwy = scenic::detail::carto2d_road_width_px(fblc, 2);
    const int w_exp = scenic::detail::carto2d_road_width_px(fblc, 3);
    expect(w_exp > w_hwy && w_hwy > w_road,
           "road width expressway > highway > road");
    expect(scenic::detail::carto2d_road_fill_color(3) !=
               scenic::detail::carto2d_road_fill_color(1),
           "expressway yellow fill vs road white");
    expect(scenic::detail::carto2d_road_casing_color(3) !=
               scenic::detail::carto2d_road_casing_color(1),
           "expressway dark casing vs road gray");
  }
  expect(std::strcmp(scenic::detail::carto2d_label_text("anno", "name", "text"),
                     "anno") == 0,
         "label_text prefers anno");
  expect(std::strcmp(scenic::detail::carto2d_label_text("", "name", "text"), "name") ==
             0,
         "label_text falls back to name");
  expect(std::strcmp(scenic::detail::carto2d_label_text("", "", "fallback"),
                     "fallback") == 0,
         "label_text falls back to text");
  expect(scenic::detail::carto2d_label_text("", "", "") == nullptr,
         "label_text all empty");
  {
    const unsigned a = scenic::detail::carto2d_boost_fill(0x00ece2d6);
    const unsigned b = scenic::detail::carto2d_boost_fill(0x00c8a0ff);
    expect(a == b, "unique-value fills collapse to one land color");
    expect(a == scenic::detail::carto2d_land_fill(), "boost_fill is land fill");
    expect(scenic::detail::carto2d_land_fill() == 0x00e9f3f5, "Baidu cream land");
    expect(scenic::detail::carto2d_map_bg() == 0x00dfd3aa, "Baidu ocean bg");
    expect(scenic::detail::carto2d_river_color() == 0x00d0a064, "soft river blue");
  }

  {
    scenic::detail::GdiCartoFrame frame;
    frame.reset(8.f, 400, 300);
    scenic::detail::MapCartoBox a = scenic::detail::carto2d_label_box(20, 20, "BJ", 14, 1);
    scenic::detail::MapCartoBox b = scenic::detail::carto2d_label_box(24, 22, "TZ", 14, 2);
    scenic::detail::MapCartoBox c = scenic::detail::carto2d_label_box(220, 40, "ELHT", 14, 2);
    expect(frame.try_keep_label(a), "higher-priority label kept");
    expect(!frame.try_keep_label(b), "overlapping lower-priority dropped");
    expect(!frame.try_keep_label(c), "LOD drops prefecture at fblc=8");
    expect(frame.label_count() == 1, "one label after country LOD");
  }

  {
    scenic::detail::GdiCartoFrame frame;
    frame.reset(22.f, 400, 300);
    scenic::detail::MapCartoBox a = scenic::detail::carto2d_label_box(20, 20, "BJ", 14, 1);
    scenic::detail::MapCartoBox far =
        scenic::detail::carto2d_label_box(220, 40, "ELHT", 14, 2);
    expect(frame.try_keep_label(a), "province at mid LOD");
    expect(frame.try_keep_label(far), "non-overlapping prefecture kept");
    expect(frame.label_count() == 2, "two labels at mid LOD");
  }

  {
    scenic::detail::GdiCartoFrame frame;
    frame.reset(80.f, 800, 600);
    for (int i = 0; i < 40; ++i) {
      const int x = 20 + (i % 10) * 18;
      const int y = 20 + (i / 10) * 22;
      scenic::detail::MapCartoBox box = scenic::detail::carto2d_label_box(x, y, "P", 14, 5);
      (void)frame.try_keep_label(box);
    }
    scenic::detail::MapCartoBox overlap =
        scenic::detail::carto2d_label_box(20, 20, "Q", 14, 5);
    expect(!frame.try_keep_label(overlap), "grid drops overlapping label");
  }

  {
    const int line[] = {0, 0, 100, 0, 100, 100};
    scenic::detail::MapCartoLineLabel pose{};
    expect(scenic::detail::carto2d_line_label_pose(line, 3, &pose),
           "line_label_pose succeeds");
    expect(pose.x == 100 && pose.y == 50, "mid segment anchor");
    expect(std::fabs(pose.angle_deg) <= 90.f + 0.01f, "upright angle range");
    expect(std::fabs(pose.angle_deg - 90.f) < 1.f, "vertical segment ~90 deg");
  }

  {
    scenic::detail::GdiCartoFrame frame;
    frame.reset(8.f, 400, 300);
    expect(frame.try_keep_point(40, 40), "first point kept");
    expect(!frame.try_keep_point(44, 42), "near point thinned");
    expect(frame.try_keep_point(200, 80), "far point kept");
    expect(frame.point_count() == 2, "two thinned points");
  }

  {
    const scenic::detail::MapCartoBox a{10, 10, 80, 28, 1};
    const scenic::detail::MapCartoBox b{20, 12, 90, 30, 2};
    expect(scenic::detail::carto2d_boxes_overlap(a, b), "overlap");
    expect(!scenic::detail::carto2d_boxes_overlap(a, {200, 40, 260, 58, 2}),
           "far boxes");
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
