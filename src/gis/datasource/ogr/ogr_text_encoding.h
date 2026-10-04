// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_TEXT_ENCODING_H_
#define SDB_DATASOURCE_GDAL_OGR_TEXT_ENCODING_H_

#include <string>

#include "gis/gis_export.h"

namespace gis {
namespace datasource {

// OGR GetFieldAsString() on Windows may be UTF-8 (GeoJSON/GPKG) or a legacy
// DBF/ACP/GBK byte string. CP_UTF8 without MB_ERR_INVALID_CHARS "succeeds"
// on GBK and yields a few random CJK crumbs — the Views / GDI label bug.
GIS_EXPORT std::wstring ogr_bytes_to_wide(const char* bytes);
GIS_EXPORT std::wstring ogr_bytes_to_wide(const std::string& bytes);

// Normalize field bytes to UTF-8 for in-memory MapScene / attribute tables.
GIS_EXPORT std::string ogr_bytes_to_utf8(const char* bytes);
GIS_EXPORT std::string ogr_bytes_to_utf8(const std::string& bytes);

}  // namespace datasource
}  // namespace gis

#endif  // SDB_DATASOURCE_GDAL_OGR_TEXT_ENCODING_H_
