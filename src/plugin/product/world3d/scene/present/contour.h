// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENE_PRESENT_CONTOUR_H_
#define PLUGIN_WORLD3D_SCENE_PRESENT_CONTOUR_H_

namespace content {
class Scene3dPresenter;
}

namespace plugin {

// Default world3d contour suite: stacked WaveHs TIN + isoline drape + side
// color-scale IR, plus globe elevation overlay when the unit Earth is on.
// Calls AtmosphereSession::apply_contour_suite_defaults().
bool present_world3d_contour_suite(content::Scene3dPresenter* scene3d);

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENE_PRESENT_CONTOUR_H_
