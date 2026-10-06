// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/pointcloud/present.h"

#include "content/public/gis_document.h"
#include "vista/assets/pointcloud/load.h"

#include <cstdint>

namespace plugin {

bool present_world3d_pointcloud(content::GisDocument* doc,
                                const std::string& path) {
  if (!doc || path.empty()) {
    return false;
  }
  vista::PointCloud cloud;
  if (!vista::load_point_cloud(path.c_str(), &cloud) || cloud.empty()) {
    return false;
  }
  const uint8_t* rgba = cloud.has_color() ? cloud.rgba.data() : nullptr;
  return doc->add_point_cloud("Point cloud", cloud.xyz.data(),
                              static_cast<int>(cloud.point_count()), rgba);
}

bool present_world3d_pointcloud_xyz(content::GisDocument* doc,
                                    const float* xyz,
                                    int point_count,
                                    const uint8_t* rgba) {
  if (!doc || !xyz || point_count <= 0) {
    return false;
  }
  return doc->add_point_cloud("Point cloud", xyz, point_count, rgba);
}

}  // namespace plugin
