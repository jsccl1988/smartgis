// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_VISTA_WORLD_POINTCLOUD_LOAD_H_
#define GIS_VISTA_WORLD_POINTCLOUD_LOAD_H_

#include "vista/vista_export.h"
#include "vista/world/pointcloud/ingest/las_io.h"
#include "vista/world/pointcloud/point_cloud.h"

namespace vista {

// Dispatch by extension: .las / .laz / .txt / .xyz / unknown→try LAS then text.
VISTA_EXPORT bool load_point_cloud(const char* path, const LasLoadOptions& options,
                                 PointCloud* out);
VISTA_EXPORT bool load_point_cloud(const char* path, PointCloud* out);

}  // namespace vista

#endif  // GIS_VISTA_WORLD_POINTCLOUD_LOAD_H_
