// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_GDAL_DRIVER_H_
#define SDB_DATASOURCE_GDAL_GDAL_DRIVER_H_

#include "sdb/datasource/gdal/ogr_export.h"

// GDAL/OGR database device (SmtSDEGdalDevice). Dataset / layers live in this
// DLL. Shared SMF codec is //src/sdb/datasource/gdal:ogr_codec. Do not vendor
// a second GDAL tree.

namespace sdb {
namespace datasource {

// Registers every GDAL/OGR driver in this SDK (GDALAllRegister).
SMT_SDE_GDAL_EXPORT bool register_gdal_driver();

}  // namespace datasource
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_GDAL_DRIVER_H_
