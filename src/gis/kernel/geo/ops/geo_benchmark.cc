// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// google/benchmark: geo::buffer vs raw OGRGeometry::Buffer (informational).

#include "gis/kernel/geo/ops/geo_ops.h"

#include "cpl_error.h"
#include "ogr_geometry.h"

#include <benchmark/benchmark.h>

namespace {

OGRPolygon make_unit_square() {
  OGRLinearRing ring;
  ring.addPoint(0.0, 0.0);
  ring.addPoint(2.0, 0.0);
  ring.addPoint(2.0, 2.0);
  ring.addPoint(0.0, 2.0);
  ring.closeRings();
  OGRPolygon poly;
  poly.addRing(&ring);
  return poly;
}

void BM_geo_buffer(benchmark::State& state) {
  const OGRPolygon poly = make_unit_square();
  constexpr double kWidth = 0.5;
  for (auto _ : state) {
    OGRGeometry* out = geo::buffer(poly, kWidth);
    benchmark::DoNotOptimize(out);
    delete out;
  }
}
BENCHMARK(BM_geo_buffer);

void BM_ogr_buffer(benchmark::State& state) {
  const OGRPolygon poly = make_unit_square();
  constexpr double kWidth = 0.5;
  constexpr int kQuadSegs = 8;
  // This GDAL build often compiles OGR::Buffer without GEOS; quiet the
  // repeated "GEOS support not enabled" CPL spam while still timing the call.
  const CPLErrorHandler prev = CPLSetErrorHandler(CPLQuietErrorHandler);
  for (auto _ : state) {
    OGRGeometry* out = poly.Buffer(kWidth, kQuadSegs);
    benchmark::DoNotOptimize(out);
    delete out;
  }
  CPLSetErrorHandler(prev);
}
BENCHMARK(BM_ogr_buffer);

}  // namespace
