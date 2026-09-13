// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/gdal_driver.h"

#include "sdb/datasource/gdal/sdbd_gdal_driver.h"

#include "gdal_priv.h"

namespace sdb {
namespace datasource {

bool register_gdal_driver() {
  GDALAllRegister();
  return register_sdbd_driver();
}

}  // namespace datasource
}  // namespace sdb
