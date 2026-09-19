// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdio>
#include <vector>

#include "legacy/render/gdi/map_carto2d.h"

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
  expect(render::carto2d_label_priority("北京市", "area", "", "110000") == 1,
         "provincial adcode priority");
  expect(render::carto2d_label_priority("南京市", "city", "", "320100") == 2,
         "prefecture adcode priority");
  expect(render::carto2d_label_priority("果洛藏族自治州", "area", "", "632600") ==
             2,
         "Qinghai prefecture adcode / 自治州");
  expect(render::carto2d_label_priority("某县", "area", "", "320102") > 3,
         "county lower than prefecture");
  expect(render::carto2d_label_priority("新疆", "region", "", "") == 1,
         "region kind");

  expect(render::carto2d_lod_max_priority(8.f) == 1,
         "country-scale LOD keeps province");
  expect(render::carto2d_lod_max_priority(22.f) == 2,
         "wide country view allows prefecture");
  expect(render::carto2d_lod_max_priority(80.f) >= 5, "street scale keeps all");

  expect(render::carto2d_point_min_distance(8.f) >= 16,
         "country-scale point spacing");
  expect(render::carto2d_point_min_distance(80.f) <= 4,
         "zoomed-in points denser");

  expect(render::carto2d_label_px(2, 22.f) >= 15, "prefecture labels larger");
  expect(render::carto2d_label_px(2, 22.f) > render::carto2d_label_px(6, 22.f),
         "prefecture larger than county");
  expect(render::carto2d_halo_px(1) >= 3, "strong halo on admin labels");
  expect(render::carto2d_point_radius(22.f) <= 2, "thin POI at regional scale");
  expect(render::carto2d_point_radius(8.f) <= 1, "thin POI at country scale");
  expect(render::carto2d_stroke_px(22.f, true) >=
             render::carto2d_stroke_px(22.f, false),
         "rivers at least as wide as admin");
  expect(render::carto2d_is_river_kind("river"), "river kind");
  expect(render::carto2d_is_road_kind("road"), "road kind");
  expect(!render::carto2d_is_road_kind("area"), "area is not a road");
  {
    const unsigned a = render::carto2d_boost_fill(0x00ece2d6);
    const unsigned b = render::carto2d_boost_fill(0x00c8a0ff);
    expect(a == b, "unique-value fills collapse to one land color");
    expect(a == render::carto2d_land_fill(), "boost_fill is land fill");
    expect(render::carto2d_land_fill() == 0x00e9f3f5, "Baidu cream land");
    expect(render::carto2d_map_bg() == 0x00dfd3aa, "Baidu ocean bg");
    expect(render::carto2d_river_color() == 0x00d0a064, "soft river blue");
  }

  {
    render::MapCarto2dFrame frame;
    frame.reset(8.f, 400, 300);
    render::MapCartoBox a = render::carto2d_label_box(20, 20, "北京", 14, 1);
    render::MapCartoBox b = render::carto2d_label_box(24, 22, "通州", 14, 2);
    render::MapCartoBox c = render::carto2d_label_box(220, 40, "乌鲁木齐", 14, 2);
    expect(frame.try_keep_label(a), "higher-priority label kept");
    expect(!frame.try_keep_label(b), "overlapping lower-priority dropped");
    expect(!frame.try_keep_label(c), "LOD drops prefecture at fblc=8");
    expect(frame.label_count() == 1, "one label after country LOD");
  }

  {
    render::MapCarto2dFrame frame;
    frame.reset(22.f, 400, 300);
    render::MapCartoBox a = render::carto2d_label_box(20, 20, "北京", 14, 1);
    render::MapCartoBox far = render::carto2d_label_box(220, 40, "乌鲁木齐", 14, 2);
    expect(frame.try_keep_label(a), "province at mid LOD");
    expect(frame.try_keep_label(far), "non-overlapping prefecture kept");
    expect(frame.label_count() == 2, "two labels at mid LOD");
  }

  {
    render::MapCarto2dFrame frame;
    frame.reset(8.f, 400, 300);
    expect(frame.try_keep_point(40, 40), "first point kept");
    expect(!frame.try_keep_point(44, 42), "near point thinned");
    expect(frame.try_keep_point(200, 80), "far point kept");
    expect(frame.point_count() == 2, "two thinned points");
  }

  {
    const render::MapCartoBox a{10, 10, 80, 28, 1};
    const render::MapCartoBox b{20, 12, 90, 30, 2};
    expect(render::carto2d_boxes_overlap(a, b), "overlap");
    expect(!render::carto2d_boxes_overlap(a, {200, 40, 260, 58, 2}),
           "far boxes");
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
