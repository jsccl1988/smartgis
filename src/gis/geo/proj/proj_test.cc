// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/geo/proj/coordinate_transform.h"

#include <cmath>
#include <cstdio>
#include <string>

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
  geo::CoordinateTransform empty("", "EPSG:3857");
  expect(!empty.is_valid(), "empty source is invalid");

  geo::CoordinateTransform bad("EPSG:4326", "EPSG:999999");
  expect(!bad.is_valid(), "invalid EPSG fails");

  geo::CoordinateTransform webmerc("EPSG:4326", "EPSG:3857");
  expect(webmerc.is_valid(), "4326->3857 compiles");

  double ox = 0.0;
  double oy = 0.0;
  expect(webmerc.transform_xy(ox, oy), "0,0 4326->3857");
  expect(std::fabs(ox) < 1e-6 && std::fabs(oy) < 1e-6,
         "0,0 stays at origin in 3857");

  double bx = 120.0;
  double by = 30.0;
  expect(webmerc.transform_xy(bx, by), "120,30 4326->3857");
  expect(std::fabs(bx - 13358338.89) < 2.0, "3857 X");
  expect(std::fabs(by - 3503549.84) < 2.0, "3857 Y");

  double tx = 116.3883;
  double ty = 39.9289;
  expect(geo::transform_xy("EPSG:4326", "EPSG:3857", tx, ty),
         "one-shot 4326->3857");
  expect(std::fabs(tx - 12956286.3) < 1.0 && std::fabs(ty - 4855615.6) < 1.0,
         "beijing metres");

  double same_x = 10.0;
  double same_y = 20.0;
  geo::CoordinateTransform ident("EPSG:4326", "EPSG:4326");
  expect(ident.is_valid() && ident.transform_xy(same_x, same_y), "identity");
  expect(std::fabs(same_x - 10.0) < 1e-9 && std::fabs(same_y - 20.0) < 1e-9,
         "identity leaves point");

  constexpr double kA = 6378140.0;
  constexpr double kB = 6356755.2882;
  const std::string longlat =
      "+proj=longlat +a=" + std::to_string(kA) + " +b=" + std::to_string(kB) +
      " +type=crs";
  const std::string tmerc =
      "+proj=tmerc +lat_0=0 +lon_0=117 +k=1 +x_0=500000 +y_0=0 +a=" +
      std::to_string(kA) + " +b=" + std::to_string(kB) + " +units=m +type=crs";
  double gx = 117.0;
  double gy = 36.0;
  expect(geo::transform_xy(longlat, tmerc, gx, gy), "IUGG1975 tmerc");
  expect(std::isfinite(gx) && std::isfinite(gy), "gauss finite");
  expect(gx > 499000.0 && gx < 501000.0, "gauss easting ~500000");
  expect(gy > 3000000.0 && gy < 5000000.0, "gauss northing");

  if (g_fails != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
