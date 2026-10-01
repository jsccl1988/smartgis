// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/runtime/analysis/playback.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void test_traffic_prefix() {
  app::AnalysisPlayback s;
  std::vector<double> xy = {0, 0, 1, 0, 2, 0, 3, 0};
  s.begin_traffic(xy, 3.0, 4, "net.geojson");
  expect(s.product() == app::AnalysisProduct::kTraffic, "product traffic");
  expect(s.frame_count() == 4, "frame_count 4");
  expect(s.set_frame_index(0), "set frame 0");
  expect(s.traffic_prefix_point_count() == 2, "prefix 2");
  expect(s.set_frame_index(3), "set frame 3");
  expect(s.traffic_prefix_point_count() == 4, "prefix 4");
  expect(s.set_frame_index(99), "clamp high");
  expect(s.frame_index() == 3, "clamped to 3");
  expect(s.playback_json().find("\"traffic\"") != std::string::npos,
         "playback json");
}

void test_flood_masks() {
  app::AnalysisPlayback s;
  const double gt[6] = {0, 1, 0, 0, 0, -1};
  s.begin_flood(2, 2, gt, 10.0, 2);
  const unsigned char a[4] = {1, 0, 0, 0};
  const unsigned char b[4] = {1, 1, 0, 0};
  s.push_flood_mask(a, 2, 2, 10.0);
  s.push_flood_mask(b, 2, 2, 11.0);
  expect(s.flood_ready(), "flood ready");
  expect(s.frame_count() == 2, "two masks");
  expect(s.flood_mask_at(1) != nullptr, "mask 1");
  expect((*s.flood_mask_at(1))[1] == 1, "mask bit");
  expect(s.flood_water_level_at(0) == 10.0, "level 0");
  expect(s.flood_water_level_at(1) == 11.0, "level 1");
}

void test_stormsurge_masks() {
  app::AnalysisPlayback s;
  const double gt[6] = {0, 1, 0, 0, 0, -1};
  s.begin_stormsurge(2, 2, gt, 12.0, 2);
  const unsigned char a[4] = {1, 0, 1, 0};
  const unsigned char b[4] = {1, 1, 1, 0};
  s.push_flood_mask(a, 2, 2, 12.0);
  s.push_flood_mask(b, 2, 2, 14.0);
  const double xyz0[] = {0, 0, 12, 1, 0, 12, 0, 1, 12};
  const int idx0[] = {0, 1, 2};
  const double xyz1[] = {0, 0, 14, 1, 0, 14, 1, 1, 14, 0, 1, 14};
  const int idx1[] = {0, 1, 2, 0, 2, 3};
  s.push_stormsurge_water_mesh(xyz0, 3, idx0, 1, 0);
  s.push_stormsurge_water_mesh(xyz1, 4, idx1, 2, 1);
  expect(s.stormsurge_ready(), "stormsurge ready");
  expect(s.product() == app::AnalysisProduct::kStormSurge, "product");
  expect(s.flood_water_level_at(1) == 14.0, "surge level 1");
  expect(s.stormsurge_water_xyz_at(0) != nullptr, "mesh xyz 0");
  expect(s.stormsurge_water_indices_at(1) != nullptr, "mesh idx 1");
  expect(s.stormsurge_water_indices_at(1)->size() == 6u, "mesh tris 1");
  expect(s.playback_json().find("\"stormsurge\"") != std::string::npos,
         "playback json");
}

void test_empty() {
  app::AnalysisPlayback s;
  expect(!s.set_frame_index(0), "empty set_frame fails");
}

}  // namespace

int main() {
  test_traffic_prefix();
  test_flood_masks();
  test_stormsurge_masks();
  test_empty();
  if (g_fails != 0) {
    std::fprintf(stderr, "analysis_playback_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::printf("analysis_playback_test: ok\n");
  return 0;
}
