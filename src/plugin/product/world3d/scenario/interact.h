// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENARIO_INTERACT_H_
#define PLUGIN_WORLD3D_SCENARIO_INTERACT_H_

namespace plugin {

// Registers Interact verbs `atmosphere_run` (default full), `world3d_run`
// (Scene3D globe suite), `orthogrid3d_run`, and `orthogrid_run` (Map2d baogrid).
void register_world3d_interact_verbs();

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENARIO_INTERACT_H_
