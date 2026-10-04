// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/seed/orbit_seed.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/showcase/plugin/common/common.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/public/map_types.h"

namespace app {
namespace detail {
namespace {

void disable_atmosphere_passes(content::Scene3dPresenter* cam) {
  if (!cam) {
    return;
  }
  cam->atmosphere_session().set_ocean_enabled(false);
  cam->atmosphere_session().set_cloud_enabled(false);
  cam->atmosphere_session().set_sky_enabled(false);
  cam->atmosphere_session().set_fog_enabled(false);
}

// Must match analysis_writers.cc hex lab pad mapping.
constexpr double kHexLabOriginLon = 116.40;
constexpr double kHexLabOriginLat = 39.90;
constexpr double kHexLabDegPerUnit = 0.025;
constexpr double kHexLabLocalSpan = 1.35;

}  // namespace

void frame_mine_orbit(content::OrbitFrame* orbit) {
  if (!orbit) {
    return;
  }
  // Beijing borehole pad only. Do NOT push_shared_extent: Map-Edit China crop
  // would replace this pad and collapse purple TIN + amber sticks to a speck.
  constexpr content::Extent2 kMine{116.34, 39.87, 116.41, 39.93};
  orbit->apply_world_extent(kMine);
  orbit->set_dolly_limits(0.55f, 8.0f);
  orbit->set_distance(1.65f);
  orbit->set_pitch(0.55f);
  orbit->set_yaw(2.59f);
}

void seed_mine_orbit(Browser& browser, content::Scene3dPresenter* cam,
                     content::OrbitFrame* orbit) {
  if (!cam || !orbit) {
    return;
  }
  // Call before mine.interpolate_stratum (clears stale DEM). After commit use
  // frame_mine_orbit so the purple TIN / amber sticks are not wiped.
  cam->abandon_mesh();
  cam->clear_overlay_tin_mesh();
  cam->clear_overlay_pointcloud();
  cam->gpu().set_wireframe_enabled(true);
  orbit->reset();
  frame_mine_orbit(orbit);
  disable_atmosphere_passes(cam);
  (void)browser;
  plugin_showcase_mark("orbit-mine");
}

void seed_stormsurge_orbit(Browser& browser, content::Scene3dPresenter* cam,
                           content::OrbitFrame* orbit) {
  if (!cam || !orbit) {
    return;
  }
  // Drop stale national DEM; present rebuild uses stormsurge DEM override.
  cam->abandon_mesh();
  cam->gpu().set_wireframe_enabled(true);
  constexpr content::Extent2 kCoast{114.15, 30.45, 114.45, 30.65};
  orbit->reset();
  orbit->apply_world_extent(kCoast);
  // Coast china_dem crop + water TIN; side pitch keeps surface+wireframe readable.
  orbit->set_distance(1.20f);
  orbit->set_pitch(0.58f);
  orbit->set_yaw(2.40f);
  browser.push_shared_extent();
  disable_atmosphere_passes(cam);
  plugin_showcase_mark("orbit-coast");
}

void frame_orthogrid3d_orbit(content::OrbitFrame* orbit) {
  if (!orbit) {
    return;
  }
  // Tight pad: hex volume must dominate the frame (not a wide DEM apron).
  const double pad = kHexLabLocalSpan * kHexLabDegPerUnit * 0.06;
  const content::Extent2 kHexLab{
      kHexLabOriginLon - pad, kHexLabOriginLat - pad,
      kHexLabOriginLon + kHexLabLocalSpan * kHexLabDegPerUnit + pad,
      kHexLabOriginLat + kHexLabLocalSpan * kHexLabDegPerUnit + pad};
  orbit->apply_world_extent(kHexLab);
  // Do NOT push_shared_extent: showcase skips select_map_tab(2), so the
  // Map-Edit 2D China crop would overwrite this hex lab orbit extent.
  // With OrbitGeoFrame spanning the lab pad to kTargetSpan, mid-range dolly
  // keeps the amber volume dominant without clipping the lattice roof.
  orbit->set_dolly_limits(0.55f, 8.0f);
  orbit->set_distance(1.35f);
  orbit->set_pitch(0.55f);
  orbit->set_yaw(2.05f);
}

void seed_orthogrid3d_orbit(Browser& browser, content::Scene3dPresenter* cam,
                            content::OrbitFrame* orbit) {
  if (!cam || !orbit) {
    return;
  }
  // Call before create_hex_grid (clears stale DEM / overlay). After commit use
  // frame_orthogrid3d_orbit so the amber TIN is not wiped.
  cam->abandon_mesh();
  cam->clear_overlay_tin_mesh();
  cam->clear_overlay_pointcloud();
  cam->gpu().set_wireframe_enabled(true);
  orbit->reset();
  frame_orthogrid3d_orbit(orbit);
  disable_atmosphere_passes(cam);
  (void)browser;
  plugin_showcase_mark("orbit-hex");
}

}  // namespace detail
}  // namespace app
