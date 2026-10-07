// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENE_POINTCLOUD_OVERLAY_H_
#define PLUGIN_WORLD3D_SCENE_POINTCLOUD_OVERLAY_H_

#include "vista/assets/pointcloud/point_cloud.h"

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace plugin {

// Lift XYZ and push RGB (authored or hypsometric-from-Z fallback) onto the
// Scene3d overlay pointcloud slot.
void apply_world3d_pointcloud_overlay(content::Scene3dPresenter* cam,
                                      const vista::PointCloud& cloud);

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENE_POINTCLOUD_OVERLAY_H_
