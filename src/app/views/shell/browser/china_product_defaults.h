// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_BROWSER_CHINA_PRODUCT_DEFAULTS_H_
#define APP_VIEWS_SHELL_BROWSER_CHINA_PRODUCT_DEFAULTS_H_

namespace app {

class Browser;

// Atmosphere toggles after apply_china_scene3d_* (for UI sync).
struct ChinaScene3dAtmoFlags {
  bool ocean = true;
  bool cloud = true;
  bool sky = true;
  bool fog = true;
};

// Shared China product defaults for interactive shell and showcase gates.
// Does NOT force GDI / ContentMapView — those stay showcase/self-test only.

// Drop china_city.style.json so default MapLibre carto paints (showcase china).
void ensure_china_maplibre_carto(Browser& browser);

// Mainland framing (content::kChinaLonLatExtent) at |view_w|x|view_h|.
// Non-China docs fall back to ViewFrame::fit_extent.
void frame_china_map2d(Browser& browser, int view_w, int view_h);

// ensure_china_maplibre_carto + frame_china_map2d + invalidate.
void apply_china_map2d_product_defaults(Browser& browser, int view_w,
                                        int view_h);

// Seed procedural rings + atmosphere.full toggles (no orbit). Honors
// SCENE3D_ATMO=0 and SCENE3D_LAND_ONLY=1.
ChinaScene3dAtmoFlags apply_china_scene3d_atmosphere(Browser& browser);

// China orbit reset + distance 2.55 (call after view3d.trackball activate).
void apply_china_scene3d_orbit(Browser& browser);

// Atmosphere + orbit (showcase full / one-shot callers). Default product face.
ChinaScene3dAtmoFlags apply_china_scene3d_product_defaults(Browser& browser);

// Leftover stereo look (opt-in): black clear, ocean sea plane, no sky/cloud/fog,
// hypsometric DEM orbit, place-name overlays. Does not change default atmosphere.
ChinaScene3dAtmoFlags apply_china_scene3d_legacy_look(Browser& browser);

}  // namespace app

#endif  // APP_VIEWS_SHELL_BROWSER_CHINA_PRODUCT_DEFAULTS_H_
