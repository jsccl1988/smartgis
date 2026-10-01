// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_VISTA_WORLD_POINTCLOUD_TEXT_IO_H_
#define GIS_VISTA_WORLD_POINTCLOUD_TEXT_IO_H_

#include "gis/gis_export.h"
#include "gis/vista/world/pointcloud/buffer/point_cloud.h"

namespace gis {

// Leftover Smt3DPointCloud text: x,z,y,r,g,b (comma). Stored as X=x, Y=y, Z=z.
GIS_EXPORT bool load_pointcloud_text(const char* path, PointCloud* out);

}  // namespace gis

#endif  // GIS_VISTA_WORLD_POINTCLOUD_TEXT_IO_H_
