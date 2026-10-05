// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_ASSETS_POINTCLOUD_TEXT_IO_H_
#define VISTA_ASSETS_POINTCLOUD_TEXT_IO_H_

#include "vista/vista_export.h"
#include "vista/assets/pointcloud/point_cloud.h"

namespace vista {

// Leftover PointCloud3d text: x,z,y,r,g,b (comma). Stored as X=x, Y=y, Z=z.
VISTA_EXPORT bool load_pointcloud_text(const char* path, PointCloud* out);

}  // namespace vista

#endif  // VISTA_ASSETS_POINTCLOUD_TEXT_IO_H_
