// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdio>
#include <cstring>

#include "gdal_priv.h"
#include "gis/datasource/gdal/gdal_driver.h"
#include "gis/map/map_layer.h"
#include "ogrsf_frmts.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

int count_features(gis::MapLayer& layer) {
  layer.reset_reading();
  int n = 0;
  for (;;) {
    gis::Feature f = layer.next_feature();
    if (!f.ogr()) {
      break;
    }
    ++n;
  }
  return n;
}

gis::MapLayer make_mem_points() {
  gis::datasource::register_gdal_driver();
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("Memory");
  expect(drv != nullptr, "Memory driver");
  if (!drv) {
    return gis::MapLayer();
  }
  GDALDataset* ds = drv->Create("map_layer_test", 0, 0, 0, GDT_Unknown, nullptr);
  expect(ds != nullptr, "Memory dataset");
  if (!ds) {
    return gis::MapLayer();
  }

  OGRSpatialReference srs;
  srs.SetWellKnownGeogCS("WGS84");
  OGRLayer* lyr = ds->CreateLayer("pts", &srs, wkbPoint, nullptr);
  expect(lyr != nullptr, "CreateLayer pts");
  if (!lyr) {
    GDALClose(ds);
    return gis::MapLayer();
  }

  OGRFieldDefn name("name", OFTString);
  lyr->CreateField(&name);

  {
    OGRFeature feat(lyr->GetLayerDefn());
    feat.SetField("name", "near");
    OGRPoint pt(1.0, 1.0);
    feat.SetGeometry(&pt);
    lyr->CreateFeature(&feat);
  }
  {
    OGRFeature feat(lyr->GetLayerDefn());
    feat.SetField("name", "far");
    OGRPoint pt(100.0, 100.0);
    feat.SetGeometry(&pt);
    lyr->CreateFeature(&feat);
  }

  return gis::MapLayer::adopt_dataset(ds, lyr);
}

void test_layer_defn_and_srs() {
  gis::MapLayer layer = make_mem_points();
  expect(layer.ogr() != nullptr, "ogr wrap");
  if (!layer.ogr()) {
    return;
  }
  expect(std::strcmp(layer.name(), "pts") == 0, "layer name");
  OGRFeatureDefn* defn = layer.layer_defn();
  expect(defn != nullptr, "layer_defn");
  if (defn) {
    expect(defn->GetFieldCount() == 1, "field count");
    expect(std::strcmp(defn->GetFieldDefn(0)->GetNameRef(), "name") == 0,
           "field name");
  }
  expect(layer.spatial_ref() != nullptr, "spatial_ref from OGRLayer");
  expect(layer.fid_column() != nullptr, "fid_column non-null");
}

void test_cursor() {
  gis::MapLayer layer = make_mem_points();
  if (!layer.ogr()) {
    return;
  }
  expect(count_features(layer) == 2, "iterate two features");
  expect(count_features(layer) == 2, "reset_reading second pass");
}

void test_spatial_filter() {
  gis::MapLayer layer = make_mem_points();
  if (!layer.ogr()) {
    return;
  }
  layer.set_spatial_filter_rect(0.0, 0.0, 2.0, 2.0);
  expect(layer.spatial_filter() != nullptr, "spatial filter set");
  expect(count_features(layer) == 1, "spatial filter keeps near");
  layer.clear_spatial_filter();
  expect(layer.spatial_filter() == nullptr, "spatial filter cleared");
  expect(count_features(layer) == 2, "clear spatial restores both");
}

void test_attribute_filter() {
  gis::MapLayer layer = make_mem_points();
  if (!layer.ogr()) {
    return;
  }
  expect(layer.set_attribute_filter("name = 'far'"), "set attribute filter");
  expect(count_features(layer) == 1, "attribute filter keeps far");
  layer.clear_attribute_filter();
  expect(count_features(layer) == 2, "clear attribute restores both");
}

}  // namespace

int main() {
  test_layer_defn_and_srs();
  test_cursor();
  test_spatial_filter();
  test_attribute_filter();

  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("map_layer_test ok\n");
  return 0;
}
