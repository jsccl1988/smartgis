// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_VISTA_WORLD_POINTCLOUD_PDAL_IO_H_
#define GIS_VISTA_WORLD_POINTCLOUD_PDAL_IO_H_

#include <cstddef>
#include <string>

#include "vista/vista_export.h"
#include "vista/world/pointcloud/point_cloud.h"

namespace vista {

// Options for a PDAL readers.las (+ optional Z range) load.
struct PdalReadOptions {
  double z_min = 0;
  double z_max = 0;
  bool has_z_range = false;
  size_t max_points = 500000;
};

// True when this binary was linked against installed PDAL (smt_has_pdal).
VISTA_EXPORT bool pdal_is_available();

// Run a PDAL pipeline JSON document into |out|.
// Pipeline must be a JSON object with a "pipeline" array (PDAL style).
VISTA_EXPORT bool run_pdal_pipeline_json(const std::string& pipeline_json,
                                       PointCloud* out);

// Convenience: readers.las on |path| with optional Z range / max_points.
VISTA_EXPORT bool run_pdal_read(const char* path, const PdalReadOptions& options,
                              PointCloud* out);

}  // namespace vista

#endif  // GIS_VISTA_WORLD_POINTCLOUD_PDAL_IO_H_
