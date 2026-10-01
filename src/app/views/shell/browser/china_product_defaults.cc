// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/china_product_defaults.h"

#include <cstdlib>
#include <string>

#include "app/views/shell/browser/browser.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "gis/present/style/style_document.h"
#include "gis/present/style/style_types.h"

namespace app {

namespace {

bool env_flag_is_one(const char* name) {
  const char* v = std::getenv(name);
  return v && v[0] == '1' && v[1] == '\0';
}

bool env_flag_is_zero(const char* name) {
  const char* v = std::getenv(name);
  return v && v[0] == '0' && v[1] == '\0';
}

ChinaScene3dAtmoFlags resolve_china_scene3d_atmo_flags() {
  ChinaScene3dAtmoFlags flags;
  // Align interactive 3D with --atmosphere-showcase=full.
  flags.ocean = true;
  flags.cloud = true;
  flags.sky = true;
  flags.fog = true;
  if (env_flag_is_zero("SMT_SCENE3D_ATMO") ||
      env_flag_is_one("SMT_SCENE3D_LAND_ONLY")) {
    flags.ocean = false;
    flags.cloud = false;
    flags.sky = false;
    flags.fog = false;
  }
  return flags;
}

}  // namespace

void ensure_china_maplibre_carto(Browser& browser) {
  content::MapScene* doc = browser.document();
  if (!doc || !doc->has_china_extent()) {
    return;
  }
  // Only drop china_city.style.json (source-layer area/line/point). Keep
  // product styles (geochem / flood / traffic / orthogrid) intact.
  const gis::style::StyleDocument* style = doc->style_document();
  if (!style) {
    return;
  }
  bool has_area_or_point = false;
  bool has_land_or_river = false;
  bool has_product_slot = false;
  for (const gis::style::StyleLayer& layer : style->layers) {
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
    doc->clear_style_document();
  }
}

void frame_china_map2d(Browser& browser, int view_w, int view_h) {
  content::MapScene* doc = browser.document();
  content::ViewFrame* frame = browser.view_frame();
  if (!doc || !frame) {
    return;
  }
  if (doc->has_china_extent()) {
    frame->apply_world_extent(content::kChinaMap2dFrameExtent, view_w, view_h);
    if (content::OrbitFrame* orbit = browser.orbit_frame()) {
      orbit->apply_world_extent(content::kChinaLonLatExtent);
    }
  } else {
    frame->fit_extent(*doc, view_w, view_h);
    if (content::OrbitFrame* orbit = browser.orbit_frame()) {
      orbit->apply_world_extent(doc->world_extent());
    }
  }
  if (content::Map2dPresenter* map2d = browser.map2d()) {
    map2d->invalidate_frame_cache();
  }
  browser.push_shared_extent();
}

void apply_china_map2d_product_defaults(Browser& browser, int view_w,
                                        int view_h) {
  ensure_china_maplibre_carto(browser);
  frame_china_map2d(browser, view_w, view_h);
  // Overlay invalidate stays in the caller (fit_map_extent). Do not call
  // browser.ui() here: a stale china_product_defaults.obj can disagree with
  // MapSession/Browser layout and load ui_ from the wrong offset (vtable AV
  // at [0x10] under parallel ninja). fit_map_extent uses matching Browser TU.
}

ChinaScene3dAtmoFlags apply_china_scene3d_atmosphere(Browser& browser) {
  const ChinaScene3dAtmoFlags flags = resolve_china_scene3d_atmo_flags();
  content::Scene3dPresenter* cam = browser.scene3d();
  if (!cam) {
    return flags;
  }
  // Product default face is atmosphere (legacy stereo is opt-in).
  cam->set_look_preset(content::Scene3dLookPreset::kAtmosphere);
  // Do not abandon_mesh on every China seed: concurrent Map-Edit FlyCube
  // present + gpu_scene_.abandon remapped heap (browse.3d 0xC0000005 on
  // select_map_tab(2)). Seed/flags alone rebuild DEM on the next present.
  cam->atmosphere_session().seed_procedural(/*with_land_rings=*/true);
  cam->atmosphere_session().set_ocean_enabled(flags.ocean);
  cam->atmosphere_session().set_cloud_enabled(flags.cloud);
  cam->atmosphere_session().set_sky_enabled(flags.sky);
  cam->atmosphere_session().set_fog_enabled(flags.fog);
  return flags;
}

void apply_china_scene3d_orbit(Browser& browser) {
  content::OrbitFrame* orbit = browser.orbit_frame();
  if (!orbit) {
    return;
  }
  orbit->reset();
  orbit->apply_world_extent(content::kChinaLonLatExtent);
  orbit->set_distance(2.55f);
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
  content::Scene3dPresenter* cam = browser.scene3d();
  if (!cam) {
    return flags;
  }
  cam->set_look_preset(content::Scene3dLookPreset::kLegacyStereo);
  cam->abandon_mesh();
  cam->atmosphere_session().seed_procedural(/*with_land_rings=*/true);
  cam->atmosphere_session().set_ocean_enabled(flags.ocean);
  cam->atmosphere_session().set_cloud_enabled(flags.cloud);
  cam->atmosphere_session().set_sky_enabled(flags.sky);
  cam->atmosphere_session().set_fog_enabled(flags.fog);
  apply_china_scene3d_orbit(browser);
  (void)cam->gpu().ensure_legacy_overlays();
  if (content::MapScene* doc = browser.document()) {
    if (doc->feature_count() > 0) {
      // Document already has china_city (or equivalent) vectors for coast gate.
      // ensure_legacy_overlays only sees coast when scene_ is bound with feats.
    }
  }
  return flags;
}

}  // namespace app
