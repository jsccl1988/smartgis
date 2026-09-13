// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_EXPORT_H_
#define SDB_DATASOURCE_GDAL_OGR_EXPORT_H_

#if defined(Export_SmtSDEGdalDevice)
#define SMT_SDE_GDAL_EXPORT __declspec(dllexport)
#else
#define SMT_SDE_GDAL_EXPORT __declspec(dllimport)
#endif

#endif  // SDB_DATASOURCE_GDAL_OGR_EXPORT_H_
