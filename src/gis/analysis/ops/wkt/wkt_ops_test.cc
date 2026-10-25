// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/ops/wkt/wkt_ops.h"

#include <cstdio>
#include <string>
#include <string_view>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

constexpr int32_t kUtm33n = 32633;
constexpr std::string_view kSquare =
    "POLYGON((500000 5000000,500100 5000000,500100 5000100,500000 5000100,500000 5000000))";
constexpr std::string_view kInner =
    "POLYGON((500020 5000020,500080 5000020,500080 5000080,500020 5000080,500020 5000020))";

}  // namespace

int main() {
  std::string out;
  std::string error;

  expect(gis::intersects(kSquare, kUtm33n, kInner, kUtm33n, &out, &error) && out == "1",
         "intersects");
  out.clear();
  error.clear();
  expect(gis::area(kSquare, kUtm33n, &out, &error) && !out.empty(), "area");
  if (!out.empty()) {
    expect(std::stod(out) > 9000.0 && std::stod(out) < 11000.0, "area near 10000");
  }

  out.clear();
  error.clear();
  expect(!gis::length("POINT(10 20)", 4326, &out, &error) && error == "geographic crs",
         "geo crs rejects length");

  out.clear();
  error.clear();
  expect(gis::is_valid(kSquare, kUtm33n, &out, &error) && out == "1", "is_valid");

  out.clear();
  error.clear();
  expect(gis::make_valid(kSquare, kUtm33n, &out, &error) && !out.empty(), "make_valid");

  out.clear();
  error.clear();
  expect(gis::point_on_surface(kSquare, kUtm33n, &out, &error) &&
             out.find("POINT") != std::string::npos,
         "point_on_surface");

  out.clear();
  error.clear();
  expect(gis::densify(kSquare, kUtm33n, "25", kUtm33n, &out, &error) && !out.empty(),
         "densify");

  out.clear();
  error.clear();
  expect(gis::covers(kSquare, kUtm33n, kInner, kUtm33n, &out, &error) && out == "1",
         "covers");

  out.clear();
  error.clear();
  expect(gis::hausdorff_distance(kSquare, kUtm33n, kInner, kUtm33n, &out, &error) &&
             std::stod(out) > 0.0,
         "hausdorff");

  out.clear();
  error.clear();
  constexpr std::string_view kSites =
      "MULTIPOINT((500000 5000000),(500100 5000000),(500000 5000100),(500100 5000100))";
  expect(gis::delaunay(kSites, kUtm33n, &out, &error) && !out.empty(), "delaunay");

  if (g_fails != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
