// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/assets/pointcloud/pdal_io.h"

namespace vista {

bool pdal_is_available() {
  return false;
}

bool run_pdal_pipeline_json(const std::string& /*pipeline_json*/,
                            PointCloud* out) {
  if (!out) {
    return false;
  }
  out->clear();
  out->error = "pdal_not_built";
  return false;
}

bool run_pdal_read(const char* path, const PdalReadOptions& /*options*/,
                   PointCloud* out) {
  if (!out) {
    return false;
  }
  out->clear();
  if (path && *path) {
    out->source_path = path;
  }
  out->error = "pdal_not_built";
  return false;
}

}  // namespace vista
