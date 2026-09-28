// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/pipeline/feature_load_pipeline.h"

#include <cstdio>
#include <string>
#include <vector>

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

namespace {

int g_fails = 0;

void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

OGRLayer* make_point_layer(GDALDataset* ds, int count) {
  OGRLayer* lyr = ds->CreateLayer("pts", nullptr, wkbPoint, nullptr);
  if (!lyr) {
    return nullptr;
  }
  for (int i = 0; i < count; ++i) {
    OGRFeature feat(lyr->GetLayerDefn());
    OGRPoint pt(static_cast<double>(i), 0.0);
    feat.SetGeometry(&pt);
    if (lyr->CreateFeature(&feat) != OGRERR_NONE) {
      return nullptr;
    }
  }
  return lyr;
}

bool run_pipeline_case(OGRLayer* lyr, size_t ordered_window, int expect_n) {
  lyr->ResetReading();
  std::vector<int> got;
  got.reserve(static_cast<size_t>(expect_n));

  gis::datasource::FeatureLoadOptions opts;
  opts.serial_threshold = 0;  // force Pipeline path
  opts.ordered_window = ordered_window;
  opts.decode_workers = 4;

  const bool ok = gis::datasource::load_ogr_layer_pipeline<int>(
      lyr,
      [](OGRFeature* feat, int* out) {
        if (!feat || !out) {
          return false;
        }
        const OGRGeometry* g = feat->GetGeometryRef();
        if (!g) {
          return false;
        }
        const OGRPoint* pt = dynamic_cast<const OGRPoint*>(g);
        if (!pt) {
          return false;
        }
        *out = static_cast<int>(pt->getX());
        return true;
      },
      [&](int&& v) { got.push_back(v); }, opts);

  if (!ok) {
    return false;
  }
  if (static_cast<int>(got.size()) != expect_n) {
    std::fprintf(stderr, "size got=%zu expect=%d window=%zu\n", got.size(),
                 expect_n, ordered_window);
    return false;
  }
  for (int i = 0; i < expect_n; ++i) {
    if (got[static_cast<size_t>(i)] != i) {
      std::fprintf(stderr, "order break at %d got=%d window=%zu\n", i,
                   got[static_cast<size_t>(i)], ordered_window);
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  GDALAllRegister();
  GDALDriver* mem = GetGDALDriverManager()->GetDriverByName("Memory");
  expect(mem != nullptr, "Memory OGR driver");
  if (!mem) {
    return 1;
  }

  GDALDataset* ds = mem->Create("", 0, 0, 0, GDT_Unknown, nullptr);
  expect(ds != nullptr, "Memory datasource");
  if (!ds) {
    return 1;
  }

  constexpr int kCount = 500;
  OGRLayer* lyr = make_point_layer(ds, kCount);
  expect(lyr != nullptr, "create point layer");
  if (!lyr) {
    GDALClose(ds);
    return 1;
  }

  expect(run_pipeline_case(lyr, /*ordered_window=*/32, kCount),
         "ordered_window=32 preserves order");
  expect(run_pipeline_case(lyr, /*ordered_window=*/0, kCount),
         "ordered_window=0 legacy preserves order");
  expect(run_pipeline_case(lyr, /*ordered_window=*/256, kCount),
         "ordered_window=256 preserves order");

  GDALClose(ds);

  if (g_fails != 0) {
    std::fprintf(stderr, "feature_load_pipeline_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::puts("feature_load_pipeline_test OK");
  return 0;
}
