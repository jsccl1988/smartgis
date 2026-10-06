// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_ORBIT_SEED_H_
#define APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_ORBIT_SEED_H_

namespace content {
class OrbitFrame;
class Scene3dPresenter;
}  // namespace content

namespace app {

class Browser;

namespace detail {

void disable_plugin_atmosphere(content::Scene3dPresenter* cam);

// Frame Beijing borehole pad; disable atmosphere passes. Clears mesh/overlays.
void seed_mine_orbit(Browser& browser, content::Scene3dPresenter* cam,
                     content::OrbitFrame* orbit);

// Re-apply borehole pad camera without clearing committed TIN / sticks.
void frame_mine_orbit(content::OrbitFrame* orbit);

// Frame Wuhan coast pad on filled china_dem; disable atmosphere; 2D labels.
void seed_stormsurge_orbit(Browser& browser, content::Scene3dPresenter* cam,
                           content::OrbitFrame* orbit);

// Re-apply coast pad camera without clearing committed water TIN / DEM.
void frame_stormsurge_orbit(content::OrbitFrame* orbit);

// Frame hex lab pad (matches commit_hex_grid_mesh geo mapping). Clears mesh.
void seed_orthogrid3d_orbit(Browser& browser, content::Scene3dPresenter* cam,
                            content::OrbitFrame* orbit);

// Re-apply hex lab camera without clearing an already-committed overlay TIN.
void frame_orthogrid3d_orbit(content::OrbitFrame* orbit);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_ORBIT_SEED_H_
