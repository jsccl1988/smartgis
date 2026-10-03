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

void seed_mine_orbit(Browser& browser, content::Scene3dPresenter* cam,
                     content::OrbitFrame* orbit) {
  if (!cam || !orbit) {
    return;
  }
  cam->abandon_mesh();
  constexpr content::Extent2 kMine{116.34, 39.87, 116.41, 39.93};
  orbit->reset();
  orbit->apply_world_extent(kMine);
  orbit->set_distance(1.65f);
  orbit->set_pitch(0.55f);
  browser.push_shared_extent();
  disable_atmosphere_passes(cam);
  plugin_showcase_mark("orbit-mine");
}

void seed_stormsurge_orbit(Browser& browser, content::Scene3dPresenter* cam,
                           content::OrbitFrame* orbit) {
  if (!cam || !orbit) {
    return;
  }
  cam->abandon_mesh();
  constexpr content::Extent2 kCoast{114.15, 30.45, 114.45, 30.65};
  orbit->reset();
  orbit->apply_world_extent(kCoast);
  orbit->set_distance(1.15f);
  orbit->set_pitch(0.72f);
  browser.push_shared_extent();
  disable_atmosphere_passes(cam);
  plugin_showcase_mark("orbit-coast");
}

void seed_orthogrid3d_orbit(Browser& browser, content::Scene3dPresenter* cam,
                            content::OrbitFrame* orbit) {
  if (!cam || !orbit) {
    return;
  }
  cam->abandon_mesh();
  const double pad = kHexLabLocalSpan * kHexLabDegPerUnit * 0.15;
  const content::Extent2 kHexLab{
      kHexLabOriginLon - pad, kHexLabOriginLat - pad,
      kHexLabOriginLon + kHexLabLocalSpan * kHexLabDegPerUnit + pad,
      kHexLabOriginLat + kHexLabLocalSpan * kHexLabDegPerUnit + pad};
  orbit->reset();
  orbit->apply_world_extent(kHexLab);
  orbit->set_distance(1.55f);
  orbit->set_pitch(0.62f);
  browser.push_shared_extent();
  disable_atmosphere_passes(cam);
  plugin_showcase_mark("orbit-hex");
}

}  // namespace detail
}  // namespace app
