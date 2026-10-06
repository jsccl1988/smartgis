// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/raster/synthetic.h"

#include "vista/terrain/dem/raster/bake_util.h"

#include <cmath>

namespace vista {
namespace detail {
namespace {

float gauss_hill(double x, double y, double cx, double cy, double sx, double sy,
                 float peak) {
  const double dx = (x - cx) / sx;
  const double dy = (y - cy) / sy;
  return peak * static_cast<float>(std::exp(-0.5 * (dx * dx + dy * dy)));
}

float hash_noise(int ix, int iy) {
  unsigned h = static_cast<unsigned>(ix * 374761393u + iy * 668265263u);
  h = (h ^ (h >> 13)) * 1274126177u;
  return static_cast<float>(h & 0xffff) / 65535.f;
}

}  // namespace

float synthetic_meters(double lon, double lat) {
  float m = 80.f;
  m += 2800.f * static_cast<float>(std::exp(
           -0.5 * ((lon - 90.0) / 14.0) * ((lon - 90.0) / 14.0) -
           0.5 * ((lat - 33.0) / 6.5) * ((lat - 33.0) / 6.5)));
  m += gauss_hill(lon, lat, 86.5, 28.0, 3.8, 1.8, 2200.f);
  m += gauss_hill(lon, lat, 91.0, 30.5, 5.5, 2.8, 1600.f);
  m += gauss_hill(lon, lat, 99.0, 28.5, 2.8, 2.2, 2400.f);
  m += gauss_hill(lon, lat, 102.5, 27.5, 2.2, 1.8, 1800.f);
  m += gauss_hill(lon, lat, 85.0, 42.5, 5.5, 1.5, 2200.f);
  m += gauss_hill(lon, lat, 88.0, 38.5, 6.0, 1.4, 1800.f);
  m += gauss_hill(lon, lat, 100.0, 38.0, 4.5, 1.6, 1400.f);
  m += gauss_hill(lon, lat, 107.5, 34.0, 3.5, 1.4, 900.f);
  m += gauss_hill(lon, lat, 112.5, 37.5, 1.8, 2.5, 700.f);
  m += gauss_hill(lon, lat, 127.5, 42.5, 2.0, 1.8, 900.f);
  m += gauss_hill(lon, lat, 121.0, 23.8, 0.55, 1.1, 2400.f);
  m += gauss_hill(lon, lat, 109.8, 19.0, 0.7, 0.5, 600.f);
  m += gauss_hill(lon, lat, 117.0, 27.5, 2.2, 1.6, 600.f);
  m -= gauss_hill(lon, lat, 84.0, 40.5, 4.5, 2.2, 2200.f);
  m -= gauss_hill(lon, lat, 87.0, 46.0, 3.5, 1.8, 1800.f);
  m -= gauss_hill(lon, lat, 105.5, 30.5, 2.5, 1.8, 1200.f);
  m -= gauss_hill(lon, lat, 116.5, 33.0, 6.0, 4.5, 400.f);
  m -= gauss_hill(lon, lat, 120.5, 31.5, 3.5, 2.0, 250.f);
  const int ix = static_cast<int>(std::floor(lon * 8.0));
  const int iy = static_cast<int>(std::floor(lat * 8.0));
  m += (hash_noise(ix, iy) - 0.5f) * 60.f;
  const bool taiwan = lon > 120.0 && lon < 122.3 && lat > 21.8 && lat < 25.5;
  const bool hainan = lon > 108.5 && lon < 111.2 && lat > 18.0 && lat < 20.2;
  const bool ocean = !taiwan && !hainan &&
                     ((lon > 122.4 && lat < 31.5) ||
                      (lon > 118.0 && lat < 21.5) || (lat < 17.5));
  if (ocean) {
    m = 0.f;
  }
  return dem_clampf(m, 0.f, 8800.f);
}

}  // namespace detail
}  // namespace vista
