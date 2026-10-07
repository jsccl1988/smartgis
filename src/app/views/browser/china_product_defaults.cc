// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/china_product_defaults.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "app/views/browser/browser.h"
#include "content/browser/session/browser_session.h"
#include "gis/style/document/style_document.h"
#include "gis/style/style_types.h"
#include "base/process/switches.h"

namespace app {

namespace {

bool env_flag_is_one(const char* name) {
  const char* v = base::switch_cstr(name);
  return v && v[0] == '1' && v[1] == '\0';
}

bool env_flag_is_zero(const char* name) {
  const char* v = base::switch_cstr(name);
  return v && v[0] == '0' && v[1] == '\0';
}

ChinaScene3dAtmoFlags resolve_china_scene3d_atmo_flags() {
  ChinaScene3dAtmoFlags flags;
  // Interactive 3D is East-China DEM + bake + labels. Full sky/cloud/fog
  // wash filled the tab with navy when DEM present lagged Init's clear.
  // SCENE3D_ATMO=1 restores atmosphere.full; SCENE3D_LAND_ONLY=1 drops ocean.
  flags.ocean = true;
  flags.cloud = false;
  flags.sky = false;
  flags.fog = false;
  if (env_flag_is_one("scene3d-atmo")) {
    flags.cloud = true;
    flags.sky = true;
    flags.fog = true;
  }
  if (env_flag_is_zero("scene3d-atmo") ||
      env_flag_is_one("scene3d-land-only")) {
    flags.ocean = false;
    flags.cloud = false;
    flags.sky = false;
    flags.fog = false;
  }
  return flags;
}

}  // namespace

void ensure_china_maplibre_carto(Browser& browser) {
  content::BrowserSession& session = browser.session();
  if (!session.document_has_china_extent()) {
    return;
  }
  // Only drop china_city.style.json (source-layer area/line/point). Keep
  // product styles (geochem / flood / traffic / orthogrid) intact.
  // Hold the shared_ptr so a concurrent Display present cannot free layers.
  const std::shared_ptr<gis::style::StyleDocument> style =
      session.style_document_shared();
  if (!style) {
    return;
  }
  bool has_area_or_point = false;
  bool has_land_or_river = false;
  bool has_product_slot = false;
  // Copy size first: a poisoned StyleDocument (stale gis_d) can AV in begin().
  const std::size_t n = style->layers.size();
  if (n > 4096) {
    return;
  }
  for (std::size_t i = 0; i < n; ++i) {
    const gis::style::StyleLayer& layer = style->layers[i];
    if (layer.source_layer == "land" || layer.source_layer == "river" ||
        layer.source_layer == "label") {
      has_land_or_river = true;
    }
    if (layer.source_layer == "area" || layer.source_layer == "point" ||
        layer.source_layer == "line") {
      has_area_or_point = true;
    }
    if (layer.source_layer.find("geochem") != std::string::npos ||
        layer.source_layer.find("flood") != std::string::npos ||
        layer.source_layer.find("traffic") != std::string::npos ||
        layer.source_layer.find("orthogrid") != std::string::npos) {
      has_product_slot = true;
    }
  }
  if (has_product_slot) {
    return;
  }
  if (has_area_or_point && !has_land_or_river) {
    session.clear_style_document();
  }
}

void frame_china_map2d(Browser& browser, int view_w, int view_h) {
  content::BrowserSession& session = browser.session();
  if (session.document_has_china_extent()) {
    session.frame_china_map2d_extents(view_w, view_h);
  } else {
    session.frame_view_and_orbit_to_document(view_w, view_h);
  }
  // Framing is extent-only when scene content is unchanged.
  session.invalidate_map2d_frame_cache();
  browser.push_shared_extent();
}

void apply_china_map2d_product_defaults(Browser& browser, int view_w,
                                        int view_h) {
  ensure_china_maplibre_carto(browser);
  frame_china_map2d(browser, view_w, view_h);
  // Overlay invalidate stays in the caller (fit_map_extent). Do not call
  // browser.ui() here: a stale china_product_defaults.obj can disagree with
  // BrowserSession/Browser layout and load ui_ from the wrong offset (vtable AV
  // at [0x10] under parallel ninja). fit_map_extent uses matching Browser TU.
}

ChinaScene3dAtmoFlags apply_china_scene3d_atmosphere(Browser& browser) {
  const ChinaScene3dAtmoFlags flags = resolve_china_scene3d_atmo_flags();
  content::BrowserSession& session = browser.session();
  // Product default face is atmosphere (legacy stereo is opt-in).
  session.set_scene3d_look_atmosphere();
  // Do not abandon_mesh on every China seed: concurrent Map-Edit FlyCube
  // present + gpu_scene_.abandon remapped heap (browse.3d 0xC0000005 on
  // select_map_tab(2)). Seed/flags alone rebuild DEM on the next present.
  // Harness: seed_procedural can AV if DEM/gpu_scene is mid-rebuild; keep
  // the call — callers must pause shell FlyCube present first.
  session.seed_scene3d_procedural(/*with_land_rings=*/true);
  session.set_scene3d_atmosphere_layers(flags.ocean, flags.cloud, flags.sky,
                                        flags.fog);
  // Interactive 3D tab is East-China DEM, not the UV globe splash (that path
  // painted a solid red sphere when albedo SRV recycled).
  session.set_scene3d_globe_enabled(false);
  // ContourSheet stacked jet TIN gives the ring-like contour surface. Soft
  // slab needs albedo alpha <=200 (see apply_contour_suite_defaults). Dark
  // isolines bake into the atlas. Fallback: DEM jet + dark curves.
  (void)session.apply_scene3d_contour_suite_or_elevation();
  // Do not fill GpuPresent::legacy_labels_ here. Tab-switch inlines used to
  // land on a skewed gpu_ and AV in vector::push_back / feature_count.
  // Software paint seeds city labels on the bound GpuPresent.
  return flags;
}

void apply_china_scene3d_orbit(Browser& browser) {
  // Fill the viewport with East-China DEM (2.55 left a postage-stamp island).
  // Trackball activate historically left yaw~0.42 (sky/navy). Default DEM
  // yaw is π-0.55; keep it after reset even if a draft already nudged yaw.
  browser.session().apply_china_scene3d_orbit();
  browser.push_shared_extent();
}

ChinaScene3dAtmoFlags apply_china_scene3d_product_defaults(Browser& browser) {
  const ChinaScene3dAtmoFlags flags = apply_china_scene3d_atmosphere(browser);
  apply_china_scene3d_orbit(browser);
  return flags;
}

ChinaScene3dAtmoFlags apply_china_scene3d_legacy_look(Browser& browser) {
  ChinaScene3dAtmoFlags flags;
  // Leftover stereo: light-blue sea + black sky; no cloud/sky/fog wash.
  flags.ocean = true;
  flags.cloud = false;
  flags.sky = false;
  flags.fog = false;
  content::BrowserSession& session = browser.session();
  std::fprintf(stderr, "china-legacy-look: preset\n");
  session.set_scene3d_look_legacy();
  // Do not abandon_mesh here: concurrent FlyCube present + gpu_scene_.abandon
  // remaps heap (0xC0000005 / ExitProcess -1 under showcase). Seed/flags alone
  // rebuild DEM on the next present — same as apply_china_scene3d_atmosphere.
  std::fprintf(stderr, "china-legacy-look: seed_procedural\n");
  session.seed_scene3d_procedural(/*with_land_rings=*/true);
  std::fprintf(stderr, "china-legacy-look: flags\n");
  session.set_scene3d_atmosphere_layers(flags.ocean, flags.cloud, flags.sky,
                                        flags.fog);
  std::fprintf(stderr, "china-legacy-look: orbit\n");
  apply_china_scene3d_orbit(browser);
  // ensure_legacy_overlays (ASCII city labels) has AVd under showcase GPU
  // present HWND + parallel DLL churn. Still paint DEM; labels composite in
  // atmosphere-showcase BMP path when overlays are available.
  if (!env_flag_is_one("atmosphere-showcase-gpu")) {
    std::fprintf(stderr, "china-legacy-look: overlays\n");
    (void)session.ensure_scene3d_legacy_overlays();
  } else {
    std::fprintf(stderr, "china-legacy-look: overlays-skipped\n");
  }
  return flags;
}

}  // namespace app
