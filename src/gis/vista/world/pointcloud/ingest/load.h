// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_VISTA_WORLD_POINTCLOUD_LOAD_H_
#define GIS_VISTA_WORLD_POINTCLOUD_LOAD_H_

#include "gis/gis_export.h"
#include "gis/vista/world/pointcloud/ingest/io/las_io.h"
#include "gis/vista/world/pointcloud/buffer/point_cloud.h"

namespace gis {

// Dispatch by extension: .las / .laz / .txt / .xyz / unknown→try LAS then text.
GIS_EXPORT bool load_point_cloud(const char* path, const LasLoadOptions& options,
                                 PointCloud* out);
GIS_EXPORT bool load_point_cloud(const char* path, PointCloud* out);

}  // namespace gis

#endif  // GIS_VISTA_WORLD_POINTCLOUD_LOAD_H_
