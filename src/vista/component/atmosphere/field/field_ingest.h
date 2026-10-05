// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_COMPONENT_ATMOSPHERE_FIELD_FIELD_INGEST_H_
#define VISTA_COMPONENT_ATMOSPHERE_FIELD_FIELD_INGEST_H_

#include <cstddef>

#include "vista/component/atmosphere/field/field_channel.h"
#include "vista/component/atmosphere/field/field_store.h"
#include "vista/vista_export.h"

namespace vista {
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
VISTA_EXPORT bool ingest_gdal_field(FieldStore* store, const char* path,
                                  FieldChannel channel,
                                  const FieldIngestOptions& opts = {});

// Batch GeoTIFF (or GDAL-openable) paths as External time slices for one
// channel. |paths| and |times| must each have |count| entries; per-slice
// time comes from |times| (opts.time_sec ignored). band/priority/kind from
// |opts|. Returns false if count==0, null args, or any slice fails (earlier
// successful slices may already be present in |store|).
VISTA_EXPORT bool ingest_gdal_field_series(FieldStore* store,
                                         FieldChannel channel,
                                         const char* const* paths,
                                         const double* times,
                                         std::size_t count,
                                         const FieldIngestOptions& opts = {});

}  // namespace atmosphere
}  // namespace vista

#endif  // VISTA_COMPONENT_ATMOSPHERE_FIELD_FIELD_INGEST_H_
