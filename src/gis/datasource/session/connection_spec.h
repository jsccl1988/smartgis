// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_SESSION_CONNECTION_SPEC_H_
#define GIS_DATASOURCE_SESSION_CONNECTION_SPEC_H_

#include <cstdint>
#include <string>

#include "gis/gis_export.h"

namespace gis {

enum DbProvider {
  PROVIDER_ACCESS,
  PROVIDER_SQLSERVER,
  PROVIDER_ORACLE,
  PROVIDER_MYSQL,
  PROVIDER_POSTGRES,
  PROVIDER_GPKG,
  PROVIDER_SPATIALITE,
  PROVIDER_SDBD,
};

enum FileProvider { PROVIDER_SHAPE, PROVIDER_OGR_SUPPORT };

enum WsProvider { PROVIDER_SMARTGIS };

enum MemProvider { PROVIDER_MEM_VER1 };

enum eDSType {
  DS_FILE_SMF,
  DS_DB_ADO,
  DS_DB_ODBC,
  DS_DB_MYSQL,
  DS_DB_ORACLE,
  DS_MEM,
  DS_WS,
};

namespace datasource {

// Routes open() to local vs remote SDBD adapters.
enum class ProviderKind { kLocalSdbd, kRemoteSdbd };

// Product connection description (OGR/session-shaped). Leftover
// DataSourceInfo conversion lives in
// legacy/gis/datasource/connection_spec_info.h.
struct GIS_EXPORT ConnectionSpec {
  ProviderKind kind = ProviderKind::kLocalSdbd;
  std::string name;
  std::string url;  // remote base / sdbd-rpc://…
  // Optional local file/db fields (empty unused).
  std::string path;
  std::string file_name;
  std::string service;
  std::string db_name;
  std::string uid;
  std::string pwd;
  std::uint32_t ds_type = 0;      // eDSType when needed
  std::uint32_t provider_id = 0;  // DbProvider / FileProvider when needed
};

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_SESSION_CONNECTION_SPEC_H_
