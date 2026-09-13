// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/gdal_driver.h"

namespace sdb {
namespace datasource {

bool register_gdal_driver() {
  // v1: seam only. Use existing //third_party/gdal_sdk from the SMF device
  // when opening formats GDAL already supports there.
  return false;
}

}  // namespace datasource
}  // namespace sdb
