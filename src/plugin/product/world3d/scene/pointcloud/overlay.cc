// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/pointcloud/overlay.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "vista/terrain/dem/raster/dem_raster.h"

namespace plugin {

void apply_world3d_pointcloud_overlay(content::Scene3dPresenter* cam,
                                      const vista::PointCloud& cloud) {
  if (!cam) {
    return;
  }
  constexpr float kElevLiftM = 120.f;
  std::vector<float> xyz_lifted = cloud.xyz;
  for (size_t i = 0; i + 2 < xyz_lifted.size(); i += 3) {
    xyz_lifted[i + 2] += kElevLiftM;
  }
  std::vector<uint8_t> rgba_fallback;
  const uint8_t* rgba = nullptr;
  if (cloud.has_color()) {
    rgba = cloud.rgba.data();
  } else {
    rgba_fallback.resize(cloud.point_count() * 4);
    for (size_t i = 0; i < cloud.point_count(); ++i) {
      const float z_m = (i * 3 + 2 < cloud.xyz.size()) ? cloud.xyz[i * 3 + 2]
                                                       : 200.f;
      float rf = 0.f;
      float gf = 0.f;
      float bf = 0.f;
      vista::hypsometric_rgb(z_m, &rf, &gf, &bf);
      rgba_fallback[i * 4] = static_cast<uint8_t>(rf * 255.f + 0.5f);
      rgba_fallback[i * 4 + 1] = static_cast<uint8_t>(gf * 255.f + 0.5f);
      rgba_fallback[i * 4 + 2] = static_cast<uint8_t>(bf * 255.f + 0.5f);
      rgba_fallback[i * 4 + 3] = 255;
    }
    rgba = rgba_fallback.data();
  }
  const int n = static_cast<int>(cloud.point_count());
  cam->set_overlay_pointcloud(xyz_lifted.data(), n, rgba);
}

}  // namespace plugin
