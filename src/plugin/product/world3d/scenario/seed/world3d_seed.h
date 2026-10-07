// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_SEED_WORLD3D_SEED_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_SEED_WORLD3D_SEED_H_

#include <cstddef>

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace plugin {

class HarnessShell;

namespace detail {

// Resolve colored DEM sample, else tiny uncolored LAS under exe.
bool resolve_world3d_pointcloud_sample(char* out_utf8, size_t out_cap);

// PLUGIN_WORLD3D_PERF_BARE=1 — strip sky/ocean/cloud/fog + pointcloud overlay
// for equal-profile timing vs leftover china DEM.
bool world3d_perf_bare_enabled();

// Showcase glue: marks + apply_world3d_east_china_face (perf-bare aware).
void seed_world3d_earth_atmosphere(HarnessShell& browser,
                                   content::Scene3dPresenter* cam);

// Showcase glue: marks + apply_world3d_true_earth_globe.
void seed_world3d_true_earth_globe(HarnessShell& browser,
                                   content::Scene3dPresenter* cam);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_SEED_WORLD3D_SEED_H_
