// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_SDBD_GDAL_DRIVER_H_
#define SDB_DATASOURCE_GDAL_SDBD_GDAL_DRIVER_H_

#include "sdb/datasource/gdal/ogr_export.h"

namespace sdb {
namespace datasource {

// mgis has no GDAL sdbd driver (HTTP :8021 only). This tree registers "SDBD"
// so layer management is GDALOpenEx("SDBD:...") / Create, not a Smt* facade.
inline constexpr char kSdbdDriverName[] = "SDBD";
inline constexpr char kSdbdPrefix[] = "SDBD:";

// Registers the SDBD driver with GDALDriverManager. Idempotent.
SMT_SDE_GDAL_EXPORT bool register_sdbd_driver();

}  // namespace datasource
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_SDBD_GDAL_DRIVER_H_
