// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/world/land_mask.h"

#include <chrono>
#include <cmath>
#include <cstdio>
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
  gis::LonLatRing box;
  box.x = {73.0, 135.0, 135.0, 73.0};
  box.y = {18.0, 18.0, 54.0, 54.0};
  expect(gis::point_in_lonlat_ring(116.0, 40.0, box), "beijing in box");
  expect(!gis::point_in_lonlat_ring(140.0, 35.0, box), "japan out");
  expect(!gis::point_in_lonlat_ring(116.0, 10.0, box),
         "south china sea out");

  gis::LonLatRing west;
  west.x = {70.0, 100.0, 100.0, 70.0};
  west.y = {20.0, 20.0, 50.0, 50.0};
  std::vector<gis::LonLatRing> rings = {west};
  expect(gis::any_ring_contains(88.0, 32.0, rings), "tibet in west");
  expect(!gis::any_ring_contains(121.0, 31.0, rings), "shanghai out");

  // china_city-scale: many high-vertex rings must not PIP every query.
  {
    std::vector<gis::LonLatRing> many;
    many.reserve(80);
    for (int i = 0; i < 80; ++i) {
      gis::LonLatRing ring;
      const double cx = 80.0 + static_cast<double>(i % 10) * 5.0;
      const double cy = 22.0 + static_cast<double>(i / 10) * 3.5;
      const int n = 120;
      ring.x.reserve(static_cast<size_t>(n));
      ring.y.reserve(static_cast<size_t>(n));
      for (int k = 0; k < n; ++k) {
        const double a = static_cast<double>(k) * 6.283185307179586 / n;
        ring.x.push_back(cx + 1.6 * std::cos(a));
        ring.y.push_back(cy + 1.2 * std::sin(a));
      }
      ring.prepare_bbox();
      many.push_back(std::move(ring));
    }
    const auto t0 = std::chrono::steady_clock::now();
    int hits = 0;
    for (int row = 0; row < 200; ++row) {
      const double lat = 54.0 - row * (54.0 - 17.5) / 199.0;
      for (int col = 0; col < 320; ++col) {
        const double lon = 73.0 + col * (135.0 - 73.0) / 319.0;
        if (gis::any_ring_contains(lon, lat, many)) {
          ++hits;
        }
      }
    }
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
    std::fprintf(stderr, "land_mask 80x120-gon x 320x200: %lld ms hits=%d\n",
                 static_cast<long long>(ms), hits);
    expect(hits > 0, "scattered rings produce land hits");
    expect(ms < 500, "prefecture-scale land_mask stays off UI-thread budget");
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d land_mask check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
