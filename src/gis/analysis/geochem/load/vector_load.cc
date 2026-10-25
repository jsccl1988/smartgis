// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geochem/load/vector_load.h"

#include <limits>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"

namespace gis {
namespace detail {

GeochemSampleSet load_vector(std::string_view path, std::string_view element) {
  GeochemSampleSet out;
  if (path.empty()) {
    out.error = "empty path";
    return out;
  }
  GDALAllRegister();
  GDALDatasetUniquePtr ds(static_cast<GDALDataset*>(GDALOpenEx(
      std::string(path).c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr,
      nullptr, nullptr)));
  if (!ds) {
    out.error = "failed to open vector";
    return out;
  }
  OGRLayer* layer = ds->GetLayer(0);
  if (!layer) {
    out.error = "no layer";
    return out;
  }
  OGRFeatureDefn* defn = layer->GetLayerDefn();
  if (!defn) {
    out.error = "no defn";
    return out;
  }

  std::vector<int> elem_fields;
  if (!element.empty()) {
    const int fi = defn->GetFieldIndex(std::string(element).c_str());
    if (fi < 0) {
      out.error = "element field missing";
      return out;
    }
    elem_fields.push_back(fi);
    out.element_names.push_back(std::string(element));
  } else {
    for (int i = 0; i < defn->GetFieldCount(); ++i) {
      OGRFieldDefn* fd = defn->GetFieldDefn(i);
      if (!fd) {
        continue;
      }
      const OGRFieldType ft = fd->GetType();
      if (ft == OFTInteger || ft == OFTInteger64 || ft == OFTReal) {
        elem_fields.push_back(i);
        out.element_names.push_back(fd->GetNameRef());
      }
    }
  }
  if (elem_fields.empty()) {
    out.error = "no numeric fields";
    return out;
  }

  const int id_fi = defn->GetFieldIndex("id");
  layer->ResetReading();
  OGRFeature* feat = nullptr;
  int auto_id = 0;
  while ((feat = layer->GetNextFeature()) != nullptr) {
    OGRGeometry* geom = feat->GetGeometryRef();
    if (!geom || geom->IsEmpty()) {
      OGRFeature::DestroyFeature(feat);
      continue;
    }
    OGRPoint pt;
    if (wkbFlatten(geom->getGeometryType()) == wkbPoint) {
      pt = *geom->toPoint();
    } else {
      OGRPoint centroid;
      if (geom->Centroid(&centroid) != OGRERR_NONE) {
        OGRFeature::DestroyFeature(feat);
        continue;
      }
      pt = centroid;
    }
    GeochemSample sample;
    sample.x = pt.getX();
    sample.y = pt.getY();
    if (id_fi >= 0 && feat->IsFieldSetAndNotNull(id_fi)) {
      sample.id = feat->GetFieldAsString(id_fi);
    } else {
      sample.id = "S" + std::to_string(++auto_id);
    }
    sample.values.resize(elem_fields.size(),
                         std::numeric_limits<double>::quiet_NaN());
    for (size_t e = 0; e < elem_fields.size(); ++e) {
      const int fi = elem_fields[e];
      if (feat->IsFieldSetAndNotNull(fi)) {
        sample.values[e] = feat->GetFieldAsDouble(fi);
      }
    }
    out.samples.push_back(std::move(sample));
    OGRFeature::DestroyFeature(feat);
  }
  if (out.samples.empty()) {
    out.error = "no samples";
    return out;
  }
  out.ok = true;
  return out;
}

}  // namespace detail
}  // namespace gis
