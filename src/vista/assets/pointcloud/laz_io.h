// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_ASSETS_POINTCLOUD_LAZ_IO_H_
#define VISTA_ASSETS_POINTCLOUD_LAZ_IO_H_

#include "vista/vista_export.h"
#include "vista/assets/pointcloud/las_io.h"
#include "vista/assets/pointcloud/point_cloud.h"

namespace vista {

// Load LAS or LAZ via static LASzip (handles compressed and uncompressed).
VISTA_EXPORT bool load_las_via_laszip(const char* path,
                                    const LasLoadOptions& options,
                                    PointCloud* out);

}  // namespace vista

#endif  // VISTA_ASSETS_POINTCLOUD_LAZ_IO_H_
