// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// google/benchmark: geo::project_point vs proj::transform_xy and a naive
// Web Mercator loop (informational).

#include "gis/kernel/proj/api/projection.h"
#include "gis/kernel/proj/backend/proj_backend.h"

#include <cmath>

#include <benchmark/benchmark.h>

using base::dbfPoint;
using geo::free_projection;
using geo::init_projection;
using geo::load_projection_string_epsg;
using geo::project_point;
using geo::Projection;

namespace {

constexpr double kWgs84Lon = 116.3883;
constexpr double kWgs84Lat = 39.9289;
constexpr double kWebMercR = 6378137.0;

// Spherical Web Mercator (EPSG:3857 sphere R=6378137); comparison baseline.
void naive_webmerc(double lon_deg, double lat_deg, double* x, double* y) {
  constexpr double kDegToRad = 0.017453292519943295769;
  const double lon = lon_deg * kDegToRad;
  const double lat = lat_deg * kDegToRad;
  *x = kWebMercR * lon;
  *y = kWebMercR * std::log(std::tan(0.78539816339744830962 + lat * 0.5));
}

void BM_project_point(benchmark::State& state) {
  Projection src = {};
  Projection dst = {};
  init_projection(&src);
  init_projection(&dst);
  load_projection_string_epsg(&src, "4326");
  load_projection_string_epsg(&dst, "3857");

  for (auto _ : state) {
    dbfPoint p(kWgs84Lon, kWgs84Lat);
    project_point(&src, &dst, &p);
    benchmark::DoNotOptimize(p);
  }

  free_projection(&src);
  free_projection(&dst);
}
BENCHMARK(BM_project_point);

void BM_transform_xy(benchmark::State& state) {
  for (auto _ : state) {
    double x = kWgs84Lon;
    double y = kWgs84Lat;
    proj::transform_xy("EPSG:4326", "EPSG:3857", &x, &y);
    benchmark::DoNotOptimize(x);
    benchmark::DoNotOptimize(y);
  }
}
BENCHMARK(BM_transform_xy);

void BM_naive_webmerc(benchmark::State& state) {
  for (auto _ : state) {
    double x = 0.0;
    double y = 0.0;
    naive_webmerc(kWgs84Lon, kWgs84Lat, &x, &y);
    benchmark::DoNotOptimize(x);
    benchmark::DoNotOptimize(y);
  }
}
BENCHMARK(BM_naive_webmerc);

}  // namespace
