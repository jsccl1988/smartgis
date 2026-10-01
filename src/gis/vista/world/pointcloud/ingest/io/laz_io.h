// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_VISTA_WORLD_POINTCLOUD_LAZ_IO_H_
#define GIS_VISTA_WORLD_POINTCLOUD_LAZ_IO_H_

#include "gis/gis_export.h"
#include "gis/vista/world/pointcloud/ingest/io/las_io.h"
#include "gis/vista/world/pointcloud/buffer/point_cloud.h"

namespace gis {

// Load LAS or LAZ via static LASzip (handles compressed and uncompressed).
GIS_EXPORT bool load_las_via_laszip(const char* path,
                                    const LasLoadOptions& options,
                                    PointCloud* out);

}  // namespace gis

#endif  // GIS_VISTA_WORLD_POINTCLOUD_LAZ_IO_H_
