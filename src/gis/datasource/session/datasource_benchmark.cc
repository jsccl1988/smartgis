// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// google/benchmark for DataSession mem open + OGR iterate. Dual label: product
// MapLayer path vs raw GDAL Memory driver.

#include <cstdint>

#include <benchmark/benchmark.h>

#include "gdal_priv.h"
#include "gis/datasource/provider/impl/gdal/gdal_driver.h"
#include "gis/datasource/session/data_session.h"
#include "ogrsf_frmts.h"

namespace {

using gis::datasource::DataSession;

bool insert_point_features(OGRLayer* lyr, int count) {
  if (!lyr || count <= 0) {
    return false;
  }
  OGRFeatureDefn* defn = lyr->GetLayerDefn();
  for (int i = 0; i < count; ++i) {
    OGRFeature feat(defn);
    OGRPoint pt(static_cast<double>(i), 0.0);
    feat.SetGeometry(&pt);
    if (lyr->CreateFeature(&feat) != OGRERR_NONE) {
      return false;
    }
  }
  return true;
}

int iterate_features(OGRLayer* lyr) {
  if (!lyr) {
    return 0;
  }
  lyr->ResetReading();
  int count = 0;
  while (OGRFeature* feat = lyr->GetNextFeature()) {
    ++count;
    OGRFeature::DestroyFeature(feat);
  }
  return count;
}

void BM_mem_create_destroy(benchmark::State& state) {
  DataSession session;
  for (auto _ : state) {
    gis::MapLayer layer = session.create_mem_vector_layer("scratch");
    benchmark::DoNotOptimize(layer);
  }
}
BENCHMARK(BM_mem_create_destroy);

void BM_mem_iterate_session(benchmark::State& state) {
  const int feature_count = static_cast<int>(state.range(0));
  DataSession session;
  gis::MapLayer layer = session.create_mem_vector_layer("bench_pts");
  OGRLayer* ogr = layer.ogr();
  if (!ogr || !insert_point_features(ogr, feature_count)) {
    state.SkipWithError("session insert failed");
    return;
  }
  for (auto _ : state) {
    const int n = iterate_features(ogr);
    benchmark::DoNotOptimize(n);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(feature_count));
}
BENCHMARK(BM_mem_iterate_session)->Arg(5000);

void BM_mem_iterate_raw(benchmark::State& state) {
  const int feature_count = static_cast<int>(state.range(0));
  GDALDriver* mem = GetGDALDriverManager()->GetDriverByName("Memory");
  if (!mem) {
    state.SkipWithError("Memory driver missing");
    return;
  }
  GDALDataset* ds = mem->Create("", 0, 0, 0, GDT_Unknown, nullptr);
  if (!ds) {
    state.SkipWithError("Memory Create failed");
    return;
  }
  OGRLayer* lyr = ds->CreateLayer("pts", nullptr, wkbPoint, nullptr);
  if (!lyr || !insert_point_features(lyr, feature_count)) {
    state.SkipWithError("raw insert failed");
    GDALClose(ds);
    return;
  }
  for (auto _ : state) {
    const int n = iterate_features(lyr);
    benchmark::DoNotOptimize(n);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(feature_count));
  GDALClose(ds);
}
BENCHMARK(BM_mem_iterate_raw)->Arg(5000);

}  // namespace

int main(int argc, char** argv) {
  gis::datasource::register_gdal_driver();
  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
    return 1;
  }
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  return 0;
}
