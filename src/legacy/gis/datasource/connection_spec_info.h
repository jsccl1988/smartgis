// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_GIS_DATASOURCE_CONNECTION_SPEC_INFO_H_
#define LEGACY_GIS_DATASOURCE_CONNECTION_SPEC_INFO_H_

#include "gis/datasource/session/connection_spec.h"
#include "gis/gis_export.h"
#include "legacy/gis/layer/layer.h"

namespace gis {
namespace datasource {

// Leftover catalog / DataSourceMgr bridge. Product headers do not export
// DataSourceInfo.
GIS_EXPORT ConnectionSpec connection_spec_from_info(
    const DataSourceInfo& info);
GIS_EXPORT DataSourceInfo connection_spec_to_info(
    const ConnectionSpec& spec);
GIS_EXPORT ProviderKind provider_kind_from_info(const DataSourceInfo& info);

}  // namespace datasource
}  // namespace gis

#endif  // LEGACY_GIS_DATASOURCE_CONNECTION_SPEC_INFO_H_
