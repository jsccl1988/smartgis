// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_WORLD3D_SEED_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_WORLD3D_SEED_H_

#include <cstddef>

#include "vista/assets/pointcloud/point_cloud.h"

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace app {

class Browser;

namespace detail {

// Resolve colored DEM sample, else tiny uncolored LAS under exe.
bool resolve_world3d_pointcloud_sample(char* out_utf8, size_t out_cap);

// PLUGIN_WORLD3D_PERF_BARE=1 — strip sky/ocean/cloud/fog + pointcloud overlay
// for equal-profile timing vs leftover china DEM.
bool world3d_perf_bare_enabled();

// China product defaults + flat DEM atmosphere (no globe).
// When perf-bare: DEM orbit only (atmosphere layers off).
void seed_world3d_earth_atmosphere(Browser& browser,
                                   content::Scene3dPresenter* cam);

// Best-effort M3 city tileset when city_root.glb is present.
void try_attach_world3d_city_tiles(content::Scene3dPresenter* cam);

// Lift XYZ and push RGB (authored or hypsometric-from-Z fallback).
void apply_world3d_pointcloud_overlay(content::Scene3dPresenter* cam,
                                     const vista::PointCloud& cloud);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_WORLD3D_SEED_H_
