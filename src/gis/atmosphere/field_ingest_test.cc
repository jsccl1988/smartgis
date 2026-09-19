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

bool write_synthetic_hs_tif(const char* path) {
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
  // Row 0 (north / lat 35..40): values 1..4; row 2 (south): 9..12.
  float data[12] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f, 10.f, 11.f,
                    12.f};
  const CPLErr err =
      band->RasterIO(GF_Write, 0, 0, 4, 3, data, 4, 3, GDT_Float32, 0, 0);
  GDALClose(ds);
  return err == CE_None;
}

}  // namespace

int main() {
  using gis::atmosphere::FieldChannel;
  using gis::atmosphere::FieldStore;
  using gis::atmosphere::ingest_gdal_field;

  const std::string path = temp_tif_path();
  expect(write_synthetic_hs_tif(path.c_str()), "write synthetic GeoTIFF");

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

  DeleteFileA(path.c_str());
  // Also remove the empty file created by GetTempFileNameA.
  std::string bare = path.substr(0, path.size() - 4);
  DeleteFileA(bare.c_str());

  if (g_fails != 0) {
    std::fprintf(stderr, "%d field_ingest check(s) failed\n", g_fails);
    return 1;
  }
  std::fprintf(stderr, "field_ingest_test OK\n");
  return 0;
}
