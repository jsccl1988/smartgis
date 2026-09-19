// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/gdal/sdbd_gdal_driver.h"

#include "gdal_priv.h"
#include "gis/datasource/gdal/sdbd_dataset.h"

namespace gis {
namespace datasource {

bool register_sdbd_driver() {
  GDALDriverManager* mgr = GetGDALDriverManager();
  if (!mgr) {
    return false;
  }
  if (mgr->GetDriverByName(kSdbdDriverName)) {
    return true;
  }

  auto* drv = new GDALDriver();
  drv->SetDescription(kSdbdDriverName);
  drv->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
  drv->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
  drv->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
  drv->SetMetadataItem(GDAL_DMD_LONGNAME,
                       "SmartGIS sdbd decorator over stock GDAL drivers");
  drv->SetMetadataItem(GDAL_DMD_CONNECTION_PREFIX, kSdbdPrefix);
  drv->SetMetadataItem(GDAL_DMD_EXTENSIONS, "");
  drv->SetMetadataItem(GDAL_DMD_HELPTOPIC, "sdbd");
  drv->pfnIdentify = SdbdDataset::identify;
  drv->pfnOpen = SdbdDataset::open;
  drv->pfnCreate = SdbdDataset::create;
  mgr->RegisterDriver(drv);
  return mgr->GetDriverByName(kSdbdDriverName) != nullptr;
}

}  // namespace datasource
}  // namespace gis
