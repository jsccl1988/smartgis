// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENARIO_INTERACT_H_
#define PLUGIN_WORLD3D_SCENARIO_INTERACT_H_

namespace plugin {

// DLL-init hook for world3d Interact scenario_op registration.
// Currently a no-op: harness IL calls run_plugin_command with command ids
// (world3d.scenario.atmosphere.* / world3d.scenario.showcase /
// orthogrid{,3d}.scenario.showcase) instead of atmosphere_run / world3d_run /
// orthogrid*_run aliases.
void register_world3d_interact_ops();

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENARIO_INTERACT_H_
