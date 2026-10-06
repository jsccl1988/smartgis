// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/seed/orbit_seed.h"

#include "plugin/runtime/host/capability/shell.h"
#include "plugin/product/world3d/scenario/common/plugin_io.h"
#include "plugin/product/stormsurge/scenario/seed.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/public/map_layer_types.h"

namespace plugin {
namespace detail {

void disable_plugin_atmosphere(content::Scene3dPresenter* cam) {
  if (!cam) {
    return;
  }
  cam->atmosphere_session().set_globe_enabled(false);
  cam->atmosphere_session().set_ocean_enabled(false);
  cam->atmosphere_session().set_cloud_enabled(false);
  cam->atmosphere_session().set_sky_enabled(false);
  cam->atmosphere_session().set_fog_enabled(false);
}

namespace {

// Must match world3d hexgrid lab pad mapping.
constexpr double kHexLabOriginLon = 116.40;
constexpr double kHexLabOriginLat = 39.90;
constexpr double kHexLabDegPerUnit = 0.025;
constexpr double kHexLabLocalSpan = 1.60;

}  // namespace

void frame_mine_orbit(content::OrbitFrame* orbit) {
  if (!orbit) {
    return;
  }
  // Beijing borehole pad only. Do NOT push_shared_extent: Map-Edit China crop
  // would replace this pad and collapse the lithology cube to a speck.
  constexpr content::Extent2 kMine{116.34, 39.87, 116.41, 39.93};
  orbit->apply_world_extent(kMine);
  orbit->set_dolly_limits(0.55f, 8.0f);
  // Pull back so the cutaway cube + layered roof read as a block, not from
  // inside the borehole sticks.
  orbit->set_distance(3.35f);
  orbit->set_pitch(0.48f);
  orbit->set_yaw(2.28f);
}

void seed_mine_orbit(HarnessShell& browser, content::Scene3dPresenter* cam,
                     content::OrbitFrame* orbit) {
  if (!cam || !orbit) {
    return;
  }
  // Call before mine.interpolate_stratum (clears stale DEM). After commit use
  // frame_mine_orbit so the lithology cube is not wiped.
  cam->abandon_mesh();
  cam->clear_overlay_tin_mesh();
  cam->clear_overlay_pointcloud();
  cam->gpu().set_wireframe_enabled(false);
  cam->gpu().set_studio_block(true);
  frame_mine_orbit(orbit);
  disable_plugin_atmosphere(cam);
  (void)browser;
  plugin_showcase_mark("orbit-mine");
}

void frame_stormsurge_orbit(content::OrbitFrame* orbit) {
  if (!orbit) {
    return;
  }
  const content::Extent2 kCoast{kStormSurgeMinLon, kStormSurgeMinLat,
                                kStormSurgeMaxLon, kStormSurgeMaxLat};
  orbit->apply_world_extent(kCoast);
  orbit->set_dolly_limits(0.55f, 8.0f);
  // Fill the borrowed shell HWND: 2.45 left mostly FlyCube navy with a tiny
  // DEM slab (inspect PNG compressed to two blobs; landish still ~0.25).
  orbit->set_distance(1.28f);
  orbit->set_pitch(0.72f);
  orbit->set_yaw(2.35f);
}

void seed_stormsurge_orbit(HarnessShell& browser, content::Scene3dPresenter* cam,
                           content::OrbitFrame* orbit) {
  if (!cam || !orbit) {
    return;
  }
  // Drop stale national DEM; present rebuilds china_dem for this coast pad.
  cam->abandon_mesh();
  cam->gpu().set_wireframe_enabled(false);
  cam->gpu().set_studio_block(false);
  orbit->reset();
  frame_stormsurge_orbit(orbit);
  browser.push_shared_extent();
  disable_plugin_atmosphere(cam);
  (void)cam->ensure_legacy_overlays();
  plugin_showcase_mark("orbit-coast");
}

void frame_orthogrid3d_orbit(content::OrbitFrame* orbit) {
  if (!orbit) {
    return;
  }
  // Tight pad: surface grid must dominate the frame (not a wide DEM apron).
  const double pad = kHexLabLocalSpan * kHexLabDegPerUnit * 0.12;
  const content::Extent2 kHexLab{
      kHexLabOriginLon - pad, kHexLabOriginLat - pad,
      kHexLabOriginLon + kHexLabLocalSpan * kHexLabDegPerUnit + pad,
      kHexLabOriginLat + kHexLabLocalSpan * kHexLabDegPerUnit + pad};
  orbit->apply_world_extent(kHexLab);
  // Do NOT push_shared_extent: showcase skips select_map_tab(2), so the
  // Map-Edit 2D China crop would overwrite this hex lab orbit extent.
  // South elevation of the hex volume (yaw=π looks from -Z). A small yaw
  // offset keeps a hint of the side wall without the underside clip of
  // the previous close 3/4 orbit.
  orbit->set_dolly_limits(0.55f, 8.0f);
  orbit->set_distance(4.20f);
  orbit->set_pitch(0.42f);
  orbit->set_yaw(3.14159265f - 0.18f);
}

void seed_orthogrid3d_orbit(HarnessShell& browser, content::Scene3dPresenter* cam,
                            content::OrbitFrame* orbit) {
  if (!cam || !orbit) {
    return;
  }
  // Call before create_hex_grid (clears stale DEM / overlay). After commit use
  // frame_orthogrid3d_orbit so the amber TIN is not wiped. Studio block matches
  // mine: cream clear + overlay volume dominates over the DEM apron.
  cam->abandon_mesh();
  cam->clear_overlay_tin_mesh();
  cam->clear_overlay_pointcloud();
  cam->gpu().set_wireframe_enabled(false);
  cam->gpu().set_studio_block(true);
  frame_orthogrid3d_orbit(orbit);
  disable_plugin_atmosphere(cam);
  (void)browser;
  plugin_showcase_mark("orbit-hex");
}

}  // namespace detail
}  // namespace plugin
