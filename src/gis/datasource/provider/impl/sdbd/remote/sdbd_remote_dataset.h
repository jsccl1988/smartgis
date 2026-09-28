// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_SDBD_REMOTE_SDBD_REMOTE_DATASET_H_
#define GIS_DATASOURCE_SDBD_REMOTE_SDBD_REMOTE_DATASET_H_

#include "gdal_priv.h"
#include "gis/gis_export.h"
#include "gis/datasource/provider/impl/sdbd/client/sdbd_client.h"
#include "gis/model/layer/layer.h"

namespace gis {
namespace datasource {

// Product facade over SdbdClient for PROVIDER_SDBD. Materializes mogu
// collections/items into an owned SDBD:MEM GDALDataset for OGR consumers.
class GIS_EXPORT SdbdRemoteDataset {
 public:
  explicit SdbdRemoteDataset(SdbdClient client);

  bool ping();  // capabilities status 200
  SdbdCallResult capabilities();
  SdbdCallResult list_collections();

  // Materialize collections (and items when available) into SDBD:MEM.
  // Caller owns the returned dataset via GDALClose.
  GDALDataset* open_as_gdal_dataset();

 private:
  SdbdClient client_;
};

// Opens PROVIDER_SDBD using info.szUrl or default HTTP/RPC endpoints.
GIS_EXPORT GDALDataset* open_provider_sdbd_dataset(
    const gis::SmtDataSourceInfo& info);

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_SDBD_REMOTE_SDBD_REMOTE_DATASET_H_
