// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_SESSION_CONNECTION_SPEC_H_
#define GIS_DATASOURCE_SESSION_CONNECTION_SPEC_H_

#include <string>

#include "gis/gis_export.h"
#include "gis/model/layer/layer.h"

namespace gis {
namespace datasource {

// Routes open() to local vs remote SDBD adapters.
enum class ProviderKind { kLocalSdbd, kRemoteSdbd };

// Product connection description. Bridges to SmtDataSourceInfo for adapters.
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
  uint ds_type = 0;      // eDSType when needed
  uint provider_id = 0;  // eSmt*Provider when needed

  SmtDataSourceInfo to_info() const;
  static ConnectionSpec from_info(const SmtDataSourceInfo& info);
  static ProviderKind kind_from_info(const SmtDataSourceInfo& info);
};

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_SESSION_CONNECTION_SPEC_H_
