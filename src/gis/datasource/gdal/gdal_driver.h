// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_GDAL_GDAL_DRIVER_H_
#define GIS_DATASOURCE_GDAL_GDAL_DRIVER_H_

#include "gis/gis_export.h"

// GDAL/OGR device (GdalDevice). Also registers the in-tree SDBD
// driver. Do not vendor a second GDAL tree.

namespace gis {
namespace datasource {

// GDALAllRegister() plus register_sdbd_driver().
GIS_EXPORT bool register_gdal_driver();

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_GDAL_GDAL_DRIVER_H_
