// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/gdal_driver.h"

#include "sdb/datasource/gdal/sdbd_gdal_driver.h"

#include "gdal_priv.h"

#include <windows.h>

#include <cstdlib>
#include <string>

namespace sdb {
namespace datasource {
namespace {

bool path_is_dir(const char* path) {
  if (!path || !path[0]) {
    return false;
  }
  const DWORD attr = GetFileAttributesA(path);
  return attr != INVALID_FILE_ATTRIBUTES &&
         (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::string module_dir() {
  char buf[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameA(/*hModule=*/nullptr, buf, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return {};
  }
  std::string path(buf, n);
  const size_t slash = path.find_last_of("\\/");
  if (slash == std::string::npos) {
    return {};
  }
  return path.substr(0, slash);
}

void set_env_if_empty(const char* name, const char* value) {
  if (!name || !value || !value[0]) {
    return;
  }
  const char* cur = std::getenv(name);
  if (cur && cur[0]) {
    return;
  }
  SetEnvironmentVariableA(name, value);
  CPLSetConfigOption(name, value);
}

// GDAL/PROJ data are not always copied next to the exe; locate install share.
void ensure_gdal_proj_data_env() {
  const std::string root = module_dir();
  if (root.empty()) {
    return;
  }

  const char* gdal_candidates[] = {
      "\\share\\gdal",
      "\\..\\third_party\\.install\\share\\gdal",
      "\\..\\..\\third_party\\.install\\share\\gdal",
  };
  for (const char* rel : gdal_candidates) {
    const std::string candidate = root + rel;
    if (path_is_dir(candidate.c_str())) {
      set_env_if_empty("GDAL_DATA", candidate.c_str());
      break;
    }
  }

  const char* proj_candidates[] = {
      "\\share\\proj",
      "",  // proj.db may sit next to the exe (proj_runtime_data copy)
      "\\..\\third_party\\.install\\share\\proj",
      "\\..\\..\\third_party\\.install\\share\\proj",
  };
  for (const char* rel : proj_candidates) {
    const std::string candidate = root + rel;
    const std::string marker = (rel[0] == '\0') ? (root + "\\proj.db")
                                                : (candidate + "\\proj.db");
    if (GetFileAttributesA(marker.c_str()) != INVALID_FILE_ATTRIBUTES) {
      const char* value =
          (rel[0] == '\0') ? root.c_str() : candidate.c_str();
      set_env_if_empty("PROJ_LIB", value);
      set_env_if_empty("PROJ_DATA", value);
      break;
    }
  }
}

}  // namespace

bool register_gdal_driver() {
  ensure_gdal_proj_data_env();
  GDALAllRegister();
  return register_sdbd_driver();
}

}  // namespace datasource
}  // namespace sdb
