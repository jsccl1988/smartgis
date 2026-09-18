// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/core/core.h"
#include "sdb/datasource/mgr/datasourcemgr.h"
#include "sdb/feature/feature.h"
#include "sdb/feature/feature_api.h"
#include "sdb/map/map.h"

#include "gdal.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <cstdio>
#include <cstring>
#include <string>

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

std::string exe_dir() {
  char path[MAX_PATH] = {};
  DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return {};
  }
  for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
    if (path[i] == '\\' || path[i] == '/') {
      path[i + 1] = '\0';
      break;
    }
  }
  return path;
}

std::string find_china_plp() {
  const std::string dir = exe_dir();
  const char* rel[] = {
      "china_plp.geojson",
      "testing\\data\\china_plp.geojson",
      "..\\testing\\data\\china_plp.geojson",
      "..\\..\\testing\\data\\china_plp.geojson",
  };
  for (const char* r : rel) {
    const std::string cand = dir + r;
    const DWORD attr = GetFileAttributesA(cand.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES &&
        (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      return cand;
    }
  }
  return {};
}

int ogr_count(OGRLayer* layer) {
  if (!layer) {
    return 0;
  }
  const int n = static_cast<int>(layer->GetFeatureCount());
  return n < 0 ? 0 : n;
}

bool layer_has_geom(OGRLayer* layer, OGRwkbGeometryType want) {
  if (!layer) {
    return false;
  }
  layer->ResetReading();
  bool hit = false;
  while (OGRFeature* feat = layer->GetNextFeature()) {
    OGRGeometry* g = feat->GetGeometryRef();
    if (g && wkbFlatten(g->getGeometryType()) == want) {
      hit = true;
    }
    OGRFeature::DestroyFeature(feat);
    if (hit) {
      break;
    }
  }
  return hit;
}

}  // namespace

int main() {
  GDALAllRegister();
  const std::string path = find_china_plp();
  expect(!path.empty(), "china_plp.geojson next to exe or testing/data");
  if (path.empty()) {
    return 1;
  }

  GDALDataset* ds = static_cast<GDALDataset*>(GDALOpenEx(
      path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr,
      nullptr));
  expect(ds != nullptr && ds->GetLayerCount() > 0, "GDALOpenEx china_plp");
  if (!ds || ds->GetLayerCount() < 1) {
    if (ds) {
      GDALClose(ds);
    }
    return 1;
  }

  sdb::SmtMap map;
  expect(map.AddLayer(ds->GetLayer(0)), "AddLayer china_plp");

  sdb::ScratchLayer point_hits = sdb::SmtDataSourceMgr::CreateMemVecLayer();
  expect(point_hits.layer != nullptr, "point scratch");
  sdb::SmtGQueryDesc gq;
  // Beijing city point in china_plp (slightly offset; needs fSmargin).
  gq.pQueryGeom = new OGRPoint(116.40, 39.91);
  gq.fSmargin = 0.05f;
  sdb::SmtPQueryDesc pq;
  int fea_type = sdb::SmtFtUnknown;
  expect(map.QueryFeature(&gq, &pq, point_hits.layer, fea_type),
         "point QueryFeature");
  expect(ogr_count(point_hits.layer) >= 1, "point-select hits china_plp");
  expect(layer_has_geom(point_hits.layer, wkbPoint),
         "point-select includes Beijing point");
  SMT_SAFE_DELETE(gq.pQueryGeom);

  sdb::ScratchLayer box_hits = sdb::SmtDataSourceMgr::CreateMemVecLayer();
  expect(box_hits.layer != nullptr, "box scratch");
  OGRLinearRing* ring = new OGRLinearRing();
  ring->addPoint(111.0, 36.0);
  ring->addPoint(119.0, 36.0);
  ring->addPoint(119.0, 41.0);
  ring->addPoint(111.0, 41.0);
  ring->closeRings();
  OGRPolygon* box = new OGRPolygon();
  box->addRingDirectly(ring);
  gq.pQueryGeom = box;
  gq.fSmargin = 0.0f;
  fea_type = sdb::SmtFtUnknown;
  expect(map.QueryFeature(&gq, &pq, box_hits.layer, fea_type),
         "box QueryFeature");
  expect(ogr_count(box_hits.layer) >= 1, "box-select hits china_plp");
  expect(layer_has_geom(box_hits.layer, wkbPolygon),
         "box-select includes Huabei polygon");
  SMT_SAFE_DELETE(gq.pQueryGeom);

  sdb::ScratchLayer flash = sdb::SmtDataSourceMgr::CreateMemVecLayer();
  expect(flash.layer != nullptr, "flash scratch");
  expect(copy_layer(flash.layer, point_hits.layer) == SMT_ERR_NONE,
         "copy_layer for flash");
  expect(ogr_count(flash.layer) >= 1, "flash copy keeps selected features");

  bool style1 = true;
  for (int frame = 0; frame < 2; ++frame) {
    style1 = !style1;
    flash.layer->ResetReading();
    int n = 0;
    while (OGRFeature* feat = flash.layer->GetNextFeature()) {
      ++n;
      OGRFeature::DestroyFeature(feat);
    }
    expect(n >= 1, style1 ? "flash frame A has features"
                          : "flash frame B has features");
  }

  sdb::SmtDataSourceMgr::DestoryMemVecLayer(point_hits);
  sdb::SmtDataSourceMgr::DestoryMemVecLayer(box_hits);
  sdb::SmtDataSourceMgr::DestoryMemVecLayer(flash);
  GDALClose(ds);
  return g_fails == 0 ? 0 : 1;
}
