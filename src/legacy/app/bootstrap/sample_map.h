// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_APP_BOOTSTRAP_SAMPLE_MAP_H_
#define LEGACY_APP_BOOTSTRAP_SAMPLE_MAP_H_

#include <string>

#include "gdal_priv.h"

namespace gis {
class DataSourceMgr;
}

namespace app {
namespace detail {

// True when |path| names an existing regular file.
bool path_is_file(const char* path);

// Prefer the prefecture pack, then the tiny PLP stub (content path policy).
bool resolve_sample_geojson(std::string* out_path);

// Open sample GeoJSON and register in the datasource manager when possible.
GDALDataset* open_or_create_sample_geojson_ds(gis::DataSourceMgr* ds_mgr);

}  // namespace detail
}  // namespace app

#endif  // LEGACY_APP_BOOTSTRAP_SAMPLE_MAP_H_
