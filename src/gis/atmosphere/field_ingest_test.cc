// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/atmosphere/field_ingest.h"

#include "gdal_priv.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void expect_near(float got, float want, float eps, const char* msg) {
  if (!(std::abs(got - want) <= eps)) {
    std::fprintf(stderr, "FAIL: %s (got=%g want=%g)\n", msg, got, want);
    ++g_fails;
  }
}

std::string temp_tif_path() {
  char dir[MAX_PATH] = {};
  char path[MAX_PATH] = {};
  GetTempPathA(MAX_PATH, dir);
  GetTempFileNameA(dir, "sgf", 0, path);
  // GetTempFileName creates an empty file; GDAL wants .tif extension.
  std::string out = path;
  out += ".tif";
  return out;
}

bool write_synthetic_hs_tif(const char* path, float fill) {
  GDALAllRegister();
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!drv) {
    return false;
  }
  GDALDataset* ds = drv->Create(path, 4, 3, 1, GDT_Float32, nullptr);
  if (!ds) {
    return false;
  }
  // North-up: origin NW, negative y pixel size.
  double gt[6] = {100.0, 5.0, 0.0, 40.0, 0.0, -5.0};
  ds->SetGeoTransform(gt);
  GDALRasterBand* band = ds->GetRasterBand(1);
  // Row 0 (north / lat 35..40): values 1..4; row 2 (south): 9..12; then scale.
  float data[12] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f, 10.f, 11.f,
                    12.f};
  for (float& v : data) {
    v *= fill;
  }
  const CPLErr err =
      band->RasterIO(GF_Write, 0, 0, 4, 3, data, 4, 3, GDT_Float32, 0, 0);
  GDALClose(ds);
  return err == CE_None;
}

void cleanup_temp_tif(const std::string& path) {
  DeleteFileA(path.c_str());
  // Also remove the empty file created by GetTempFileNameA.
  std::string bare = path.substr(0, path.size() - 4);
  DeleteFileA(bare.c_str());
}

}  // namespace

int main() {
  using gis::atmosphere::FieldChannel;
  using gis::atmosphere::FieldIngestOptions;
  using gis::atmosphere::FieldStore;
  using gis::atmosphere::ingest_gdal_field;
  using gis::atmosphere::ingest_gdal_field_series;

  const std::string path = temp_tif_path();
  expect(write_synthetic_hs_tif(path.c_str(), 1.f), "write synthetic GeoTIFF");

  FieldStore store;
  expect(ingest_gdal_field(&store, path.c_str(), FieldChannel::kWaveHs),
         "ingest GeoTIFF → kWaveHs");
  expect(store.layer_count() == 1, "one ingested layer");

  // After flip, SW corner (min_lon, min_lat) is GDAL south-west = 9.
  expect_near(store.sample(FieldChannel::kWaveHs, 100.0, 25.0, 0.0), 9.f, 0.2f,
              "SW after north-up flip");
  // NE (max_lon, max_lat) ≈ GDAL north-east = 4.
  expect_near(store.sample(FieldChannel::kWaveHs, 120.0, 40.0, 0.0), 4.f, 0.2f,
              "NE after north-up flip");

  expect(!ingest_gdal_field(&store, "Z:/no/such/file.tif", FieldChannel::kWindU),
         "missing path fails");

  cleanup_temp_tif(path);

  // Batch time series: two GeoTIFF slices → temporal lerp on cover.
  {
    const std::string p0 = temp_tif_path();
    const std::string p1 = temp_tif_path();
    expect(write_synthetic_hs_tif(p0.c_str(), 1.f), "write series t0");
    expect(write_synthetic_hs_tif(p1.c_str(), 2.f), "write series t1");

    FieldStore series;
    const char* paths[] = {p0.c_str(), p1.c_str()};
    const double times[] = {0.0, 10.0};
    FieldIngestOptions opts;
    opts.priority = 10;
    expect(ingest_gdal_field_series(&series, FieldChannel::kWaveHs, paths,
                                    times, 2, opts),
           "ingest series");
    expect(series.layer_count() == 2, "two series slices");

    // SW at t0 ≈ 9, at t1 ≈ 18; mid ≈ 13.5
    expect_near(series.sample(FieldChannel::kWaveHs, 100.0, 25.0, 0.0), 9.f,
                0.3f, "series t0 SW");
    expect_near(series.sample(FieldChannel::kWaveHs, 100.0, 25.0, 10.0), 18.f,
                0.3f, "series t1 SW");
    expect_near(series.sample(FieldChannel::kWaveHs, 100.0, 25.0, 5.0), 13.5f,
                0.4f, "series mid lerp SW");

    double t_min = 0.0;
    double t_max = 0.0;
    expect(series.timed_slice_range(FieldChannel::kWaveHs, &t_min, &t_max),
           "series timed range");
    expect(t_min == 0.0 && t_max == 10.0, "series range 0..10");

    expect(!ingest_gdal_field_series(&series, FieldChannel::kWaveHs, nullptr,
                                    times, 2, opts),
           "null paths fails");
    expect(!ingest_gdal_field_series(&series, FieldChannel::kWaveHs, paths,
                                    times, 0, opts),
           "empty count fails");

    cleanup_temp_tif(p0);
    cleanup_temp_tif(p1);
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d field_ingest check(s) failed\n", g_fails);
    return 1;
  }
  std::fprintf(stderr, "field_ingest_test OK\n");
  return 0;
}
