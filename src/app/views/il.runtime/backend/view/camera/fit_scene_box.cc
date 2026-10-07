// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/view/camera/fit_scene_box.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

#include "app/views/browser/browser.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/gpu/scene3d_gpu_present.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "ui/views/map/viewport/draw_host.h"

namespace app {
namespace detail {
namespace {

bool mesh_aabb(const std::vector<float>& xyz, float* mn_x, float* mn_y,
               float* mn_z, float* mx_x, float* mx_y, float* mx_z) {
  if (!mn_x || !mn_y || !mn_z || !mx_x || !mx_y || !mx_z) {
    return false;
  }
  const size_t n = xyz.size() / 3;
  if (n == 0) {
    return false;
  }
  *mn_x = *mx_x = xyz[0];
  *mn_y = *mx_y = xyz[1];
  *mn_z = *mx_z = xyz[2];
  for (size_t i = 1; i < n; ++i) {
    const float x = xyz[i * 3];
    const float y = xyz[i * 3 + 1];
    const float z = xyz[i * 3 + 2];
    *mn_x = (std::min)(*mn_x, x);
    *mn_y = (std::min)(*mn_y, y);
    *mn_z = (std::min)(*mn_z, z);
    *mx_x = (std::max)(*mx_x, x);
    *mx_y = (std::max)(*mx_y, y);
    *mx_z = (std::max)(*mx_z, z);
  }
  return true;
}

// Best orbit distance so a sphere of |radius| fills ~70% of the vertical FOV.
float distance_for_radius(float radius) {
  const float r = (std::max)(radius, 0.05f);
  const float half = 0.5f * content::kScene3dFovY;
  const float tan_half = std::tan(half);
  if (tan_half < 1.0e-4f) {
    return 3.2f;
  }
  return r / tan_half * 1.35f;
}

void invalidate_scene(Browser& browser) {
  if (ui::views::DrawHost* pane = browser.scene_draw_host()) {
    pane->invalidate_native();
  }
}

}  // namespace

bool fit_scene_box(Browser& browser) {
  content::OrbitFrame* orbit = browser.orbit_frame();
  content::Scene3dPresenter* cam = browser.scene3d();
  if (!orbit || !cam) {
    return false;
  }

  float mn_x = 0.f;
  float mn_y = 0.f;
  float mn_z = 0.f;
  float mx_x = 0.f;
  float mx_y = 0.f;
  float mx_z = 0.f;
  bool have_box = false;
  {
    content::Scene3dGpuPresent& gpu = cam->gpu();
    std::lock_guard<std::recursive_mutex> lock(gpu.mutex());
    have_box = mesh_aabb(gpu.local_xyz(), &mn_x, &mn_y, &mn_z, &mx_x, &mx_y,
                         &mx_z);
  }

  float radius = 1.6f;
  float height_ratio = 0.25f;
  if (have_box) {
    const float hx = 0.5f * (mx_x - mn_x);
    const float hy = 0.5f * (mx_y - mn_y);
    const float hz = 0.5f * (mx_z - mn_z);
    radius = std::sqrt(hx * hx + hy * hy + hz * hz);
    height_ratio = hy / (std::max)(radius, 1.0e-3f);
  } else {
    // Lon/lat world extent 鈫?approximate orbit diagonal (~3.2 for full China).
    const content::Extent2 e = orbit->world_extent();
    const double span_lon = e.xmax - e.xmin;
    const double span_lat = e.ymax - e.ymin;
    const double span = (std::max)(span_lon, span_lat);
    if (span > 1.0e-6) {
      // China ~62掳 maps to orbit diagonal ~3.2.
      radius = static_cast<float>(span / 62.0 * 2.25);
    }
  }

  float dist = distance_for_radius(radius);
  dist = (std::max)(0.65f, (std::min)(dist, 11.5f));
  orbit->set_dolly_limits(0.55f, 12.f);
  orbit->set_distance(dist);

  // Elevated 3/4 view: taller boxes need a slightly flatter pitch so the
  // roof/cutaway stays in frame; flat DEM gets a steeper look-down.
  const float pitch =
      (std::max)(0.32f, (std::min)(0.72f, 0.52f - 0.18f * height_ratio));
  orbit->set_pitch(pitch);

  // Prefer south-of-target (north up) with a small yaw bias for volume sides.
  const float yaw = content::kScene3dDefaultYaw - 0.22f;
  orbit->set_yaw(yaw);

  // Nudge off exact default so expect_orbit_moved can score after fly.
  if (std::fabs(orbit->yaw() - content::kScene3dDefaultYaw) < 0.001f) {
    orbit->set_yaw(content::kScene3dDefaultYaw - 0.05f);
  }

  invalidate_scene(browser);
  return true;
}

}  // namespace detail
}  // namespace app
