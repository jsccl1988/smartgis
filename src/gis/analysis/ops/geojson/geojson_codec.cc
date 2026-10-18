// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/ops/geojson/geojson_codec.h"

#include <memory>
#include <string>

#include "gdal_priv.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

namespace gis {
namespace detail {

bool geometry_to_geojson(const std::string& path, OGRGeometry* geom) {
  if (!geom || path.empty()) {
    return false;
  }
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
  if (!driver) {
    return false;
  }
  VSIUnlink(path.c_str());
  GDALDatasetUniquePtr ds(driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
  if (!ds) {
    return false;
  }
  OGRLayer* layer = ds->CreateLayer("result", nullptr, wkbUnknown, nullptr);
  if (!layer) {
    return false;
  }
  OGRFeatureDefn* defn = layer->GetLayerDefn();
  OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(defn));
  if (!feat) {
    return false;
  }
  feat->SetGeometry(geom);
  return layer->CreateFeature(feat.get()) == OGRERR_NONE;
}

std::unique_ptr<OGRGeometry> geometry_from_geojson(const std::string& path) {
  GDALAllRegister();
  GDALDatasetUniquePtr ds(static_cast<GDALDataset*>(GDALOpenEx(
      path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr, nullptr)));
  if (!ds) {
    return nullptr;
  }
  for (int i = 0; i < ds->GetLayerCount(); ++i) {
    OGRLayer* layer = ds->GetLayer(i);
    if (!layer) {
      continue;
    }
    layer->ResetReading();
    OGRFeature* feat = nullptr;
    while ((feat = layer->GetNextFeature()) != nullptr) {
      OGRGeometry* geom = feat->GetGeometryRef();
      if (geom && !geom->IsEmpty()) {
        OGRGeometry* clone = geom->clone();
        OGRFeature::DestroyFeature(feat);
        return std::unique_ptr<OGRGeometry>(clone);
      }
      OGRFeature::DestroyFeature(feat);
    }
  }
  return nullptr;
}

}  // namespace detail
}  // namespace gis
