// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_GDAL_DRIVER_H_
#define SDB_DATASOURCE_GDAL_GDAL_DRIVER_H_

#include "sdb/datasource/gdal/ogr_export.h"

// GDAL/OGR device (SmtSDEGdalDevice). Also registers the in-tree SDBD
// driver. Do not vendor a second GDAL tree.

namespace sdb {
namespace datasource {

// GDALAllRegister() plus register_sdbd_driver().
SDE_GDAL_EXPORT bool register_gdal_driver();

}  // namespace datasource
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_GDAL_DRIVER_H_
