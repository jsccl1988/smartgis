// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/sdbd/sdbd_remote_dataset.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "cpl_vsi.h"
#include "gis/datasource/ogr/ogr_connect.h"
#include "gis/datasource/sdbd/sdbd_dataset.h"
#include "gis/datasource/sdbd/sdbd_json.h"
#include "gis/map/layer_kind.h"
#include "ogrsf_frmts.h"

#include <cstring>

namespace gis {
namespace datasource {
namespace {

void copy_layer_features(OGRLayer* src, OGRLayer* dst) {
  if (!src || !dst) {
    return;
  }
  // TODO(execution): reuse gis::datasource::load_ogr_layer_pipeline when
  // clone/CreateFeature decode is factored (second-wave SDBD adoption).
  src->ResetReading();
  while (OGRFeature* feat = src->GetNextFeature()) {
    OGRFeature* clone = feat->Clone();
    if (clone) {
      dst->CreateFeature(clone);
      OGRFeature::DestroyFeature(clone);
    }
    OGRFeature::DestroyFeature(feat);
  }
}

// Best-effort: treat items body as GeoJSON and copy into dst under layer_name.
bool try_ingest_geojson_items(GDALDataset* dst, const std::string& layer_name,
                              const std::string& body) {
  if (!dst || body.empty() || layer_name.empty()) {
    return false;
  }
  static int seq = 0;
  char path[128];
  std::snprintf(path, sizeof(path), "/vsimem/sdbd_remote_%d.json", ++seq);

  GByte* buf = static_cast<GByte*>(VSIMalloc(body.size() + 1));
  if (!buf) {
    return false;
  }
  std::memcpy(buf, body.data(), body.size());
  buf[body.size()] = '\0';
  VSILFILE* fp = VSIFileFromMemBuffer(
      path, buf, static_cast<vsi_l_offset>(body.size()), /*bTakeOwnership=*/TRUE);
  if (!fp) {
    VSIFree(buf);
    return false;
  }
  VSIFCloseL(fp);

  GDALDataset* gj = static_cast<GDALDataset*>(
      GDALOpenEx(path, GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr,
                 nullptr));
  if (!gj) {
    VSIUnlink(path);
    return false;
  }

  OGRLayer* src = gj->GetLayerCount() > 0 ? gj->GetLayer(0) : nullptr;
  if (!src) {
    GDALClose(gj);
    VSIUnlink(path);
    return false;
  }

  OGRLayer* lyr =
      dst->CreateLayer(layer_name.c_str(), src->GetSpatialRef(),
                       src->GetGeomType(), nullptr);
  if (!lyr) {
    GDALClose(gj);
    VSIUnlink(path);
    return false;
  }
  OGRFeatureDefn* defn = src->GetLayerDefn();
  for (int i = 0; i < defn->GetFieldCount(); ++i) {
    lyr->CreateField(defn->GetFieldDefn(i));
  }
  copy_layer_features(src, lyr);
  GDALClose(gj);
  VSIUnlink(path);
  return true;
}

}  // namespace

SdbdRemoteDataset::SdbdRemoteDataset(SdbdClient client)
    : client_(std::move(client)) {}

bool SdbdRemoteDataset::ping() {
  const SdbdCallResult caps = client_.get_capabilities(/*timeout_sec=*/5);
  return caps.ok && caps.status == 200;
}

SdbdCallResult SdbdRemoteDataset::capabilities() {
  return client_.get_capabilities(/*timeout_sec=*/5);
}

SdbdCallResult SdbdRemoteDataset::list_collections() {
  return client_.get_collections(/*timeout_sec=*/10);
}

GDALDataset* SdbdRemoteDataset::open_as_gdal_dataset() {
  if (!ping()) {
    return nullptr;
  }

  ConnectionSpec mem_spec;
  mem_spec.ds_type = DS_MEM;
  mem_spec.provider_id = PROVIDER_MEM_VER1;
  mem_spec.name = "sdbd_remote";
  GDALDataset* ds = open_sdbd_dataset(mem_spec);
  if (!ds) {
    return nullptr;
  }

  const SdbdCallResult cols = list_collections();
  if (!(cols.ok && cols.status == 200)) {
    // Empty MEM still proves the remote ping succeeded.
    return ds;
  }

  const std::vector<std::string> ids = parse_collection_ids(cols.body);
  for (const std::string& id : ids) {
    const SdbdCallResult items =
        client_.get_collection_items(id, /*query=*/"", /*timeout_sec=*/30);
    if (items.ok && items.status == 200 &&
        try_ingest_geojson_items(ds, id, items.body)) {
      continue;
    }
    // Ensure GetLayerByName works even when items are empty / non-GeoJSON.
    if (!ds->GetLayerByName(id.c_str())) {
      ds->CreateLayer(id.c_str(), nullptr, wkbUnknown, nullptr);
    }
  }
  return ds;
}

GDALDataset* open_provider_sdbd_dataset(const ConnectionSpec& spec) {
  const std::string conn = sdbd_base_url(spec);
  SdbdRemoteDataset remote{SdbdClient{conn}};
  return remote.open_as_gdal_dataset();
}

}  // namespace datasource
}  // namespace gis
