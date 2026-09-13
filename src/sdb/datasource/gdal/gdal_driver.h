// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_GDAL_DRIVER_H_
#define SDB_DATASOURCE_GDAL_GDAL_DRIVER_H_

// GDAL/OGR extension seam (QGIS providers/ogr, GDAL Dataset / OGRSFDriver).
//
// Mapping:
//   datasource  ≈ GDALDataset / OGRSFDriver
//   layer       ≈ OGRLayer
//   feature     ≈ OGRFeature + product fields
//   map         = document that holds layers (not a GDAL object)
//
// SMF already opens some raster/vector paths via //third_party/gdal_sdk.
// That is not a full OGR feature model. A future driver registers beside
// ado/mem/smf/ws through SmtSDEDeviceMgr without changing content/public.
// Do not vendor a second GDAL tree.

namespace sdb {
namespace datasource {

// Registers every GDAL/OGR driver in this SDK (GDALAllRegister).
bool register_gdal_driver();

}  // namespace datasource
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_GDAL_DRIVER_H_
