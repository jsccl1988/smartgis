// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// google/benchmark: geo::transform_xy vs a reused CoordinateTransform
// and a naive Web Mercator loop (informational).

#include "gis/geo/proj/coordinate_transform.h"

#include <cmath>

#include <benchmark/benchmark.h>

namespace {

constexpr double kWgs84Lon = 116.3883;
constexpr double kWgs84Lat = 39.9289;
constexpr double kWebMercR = 6378137.0;

void naive_webmerc(double lon_deg, double lat_deg, double* x, double* y) {
  constexpr double kDegToRad = 0.017453292519943295769;
  const double lon = lon_deg * kDegToRad;
  const double lat = lat_deg * kDegToRad;
  *x = kWebMercR * lon;
  *y = kWebMercR * std::log(std::tan(0.78539816339744830962 + lat * 0.5));
}

void BM_transform_xy_oneshot(benchmark::State& state) {
  for (auto _ : state) {
    double x = kWgs84Lon;
    double y = kWgs84Lat;
    geo::transform_xy("EPSG:4326", "EPSG:3857", x, y);
    benchmark::DoNotOptimize(x);
    benchmark::DoNotOptimize(y);
  }
}
BENCHMARK(BM_transform_xy_oneshot);

void BM_coordinate_transform(benchmark::State& state) {
  geo::CoordinateTransform pipeline("EPSG:4326", "EPSG:3857");
  for (auto _ : state) {
    double x = kWgs84Lon;
    double y = kWgs84Lat;
    pipeline.transform_xy(x, y);
    benchmark::DoNotOptimize(x);
    benchmark::DoNotOptimize(y);
  }
}
BENCHMARK(BM_coordinate_transform);

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
