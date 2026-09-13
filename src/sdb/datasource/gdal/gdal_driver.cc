// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/gdal_driver.h"

#include "gdal_priv.h"

namespace sdb {
namespace datasource {

bool register_gdal_driver() {
  GDALAllRegister();
  return true;
}

}  // namespace datasource
}  // namespace sdb
