// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_LEGACY_SEED_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_LEGACY_SEED_H_

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace plugin {

class HarnessShell;

namespace detail {

// Seeds leftover stereo look + china_city open for --atmosphere-showcase=legacy.
// Returns 0 on success, else an exit code (typically 53).
int seed_atmosphere_legacy_mode(HarnessShell& browser,
                                content::Scene3dPresenter* cam);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_LEGACY_SEED_H_
