// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ATMOSPHERE_FIELD_INGEST_H_
#define GIS_ATMOSPHERE_FIELD_INGEST_H_

#include "gis/atmosphere/field_channel.h"
#include "gis/atmosphere/field_store.h"
#include "gis/gis_export.h"

namespace gis {
namespace atmosphere {

// Options for GDAL single-band → FieldChannel ingest (GeoTIFF / MEM / etc.).
struct FieldIngestOptions {
  int band = 1;
  int priority = 10;
  FieldSourceKind kind = FieldSourceKind::kExternal;
  // NaN = timeless layer; otherwise stored for temporal lerp.
  double time_sec = 0.0;
};

// Opens |path| via GDAL, reads one band into |channel| on |store|.
// Works for GeoTIFF and any format GDAL can open (NetCDF/GRIB when drivers
// are present). No custom decoder. Returns false on open/read failure.
GIS_EXPORT bool ingest_gdal_field(FieldStore* store, const char* path,
                                  FieldChannel channel,
                                  const FieldIngestOptions& opts = {});

}  // namespace atmosphere
}  // namespace gis

#endif  // GIS_ATMOSPHERE_FIELD_INGEST_H_
