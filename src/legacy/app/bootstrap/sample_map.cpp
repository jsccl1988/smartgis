// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/app/bootstrap/sample_map.h"

#include <string>

#include "base/core/log.h"
#include "content/public/map_bootstrap.h"
#include "legacy/core/util/path.h"
#include "legacy/gis/datasource/datasource_mgr.h"
#include "ogrsf_frmts.h"

using namespace gis;

namespace app {
namespace detail {

bool path_is_file(const char* path) {
  if (!path || !path[0]) {
    return false;
  }
  const DWORD attr = GetFileAttributesA(path);
  return attr != INVALID_FILE_ATTRIBUTES &&
         (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool resolve_sample_geojson(std::string* out_path) {
  const std::string app = get_app_path();
  return content::try_resolve_existing_sample_map({app}, out_path);
}

GDALDataset* open_or_create_sample_geojson_ds(DataSourceMgr* ds_mgr) {
  if (!ds_mgr) {
    return nullptr;
  }
  if (GDALDataset* existing = ds_mgr->get_data_source("china_city")) {
    return existing;
  }
  std::string geojson_path;
  if (!resolve_sample_geojson(&geojson_path)) {
    LOGGING(LOG_WARNING,
            "China sample map not found (out/data/china_city.*).");
    return nullptr;
  }

  char szPath[_MAX_PATH] = {};
  char szFileName[_MAX_PATH] = {};
  char szTitle[_MAX_PATH] = {};
  char szExt[_MAX_PATH] = {};
  split_file_name(geojson_path.c_str(), szPath, szFileName, szTitle, szExt);

  DataSourceInfo info;
  info.unType = DS_FILE_SMF;
  info.unProvider = PROVIDER_OGR_SUPPORT;
  strcpy_s(info.szName, "china_city");
  strncpy_s(info.file.szPath, szPath, _TRUNCATE);
  strncpy_s(info.file.szFileName, szFileName, _TRUNCATE);

  // One open: prefer the mgr so Init / DelayInit share the same handle.
  if (GDALDataset* via_mgr = ds_mgr->create_data_source(info)) {
    LOGGING(LOG_INFO, "Opened sample GeoJSON datasource: %s",
            geojson_path.c_str());
    return via_mgr;
  }

  GDALDataset* direct = static_cast<GDALDataset*>(
      GDALOpenEx(geojson_path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY,
                 nullptr, nullptr, nullptr));
  if (!direct) {
    LOGGING(LOG_ERROR, "GDALOpenEx failed for sample GeoJSON: %s",
            geojson_path.c_str());
    return nullptr;
  }
  LOGGING(LOG_INFO,
          "Sample GeoJSON open via GDALOpenEx (mgr register skipped): %s",
          geojson_path.c_str());
  return direct;
}

}  // namespace detail
}  // namespace app
