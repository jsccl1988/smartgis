// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/session/data_session.h"

#include "gis/datasource/gdal/gdal_driver.h"
#include "ogrsf_frmts.h"

namespace gis {
namespace datasource {

DataSession::DataSession() : registry_(ProviderRegistry::make_default()) {}

DataSession::DataSession(ProviderRegistry registry)
    : registry_(std::move(registry)) {}

DatasetHandle DataSession::open(const ConnectionSpec& spec) {
  register_gdal_driver();
  return registry_.open(spec);
}

MapLayer DataSession::create_mem_vector_layer(const char* name) {
  const char* layer_name = (name && name[0] != '\0') ? name : "scratch";

  ConnectionSpec spec;
  spec.kind = ProviderKind::kLocalSdbd;
  spec.ds_type = DS_MEM;
  spec.provider_id = PROVIDER_MEM_VER1;
  spec.name = layer_name;

  DatasetHandle handle = open(spec);
  if (!handle) {
    return MapLayer();
  }

  GDALDataset* ds = handle.gdal();
  OGRLayer* layer =
      ds->CreateLayer(layer_name, nullptr, wkbUnknown, nullptr);
  if (layer) {
    OGRFieldDefn style("style", OFTBinary);
    layer->CreateField(&style);
  }

  GDALDataset* owned = handle.release();
  return MapLayer::adopt_dataset(owned, layer);
}

}  // namespace datasource
}  // namespace gis
