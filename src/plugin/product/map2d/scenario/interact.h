// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_SCENARIO_INTERACT_H_
#define PLUGIN_MAP2D_SCENARIO_INTERACT_H_

namespace plugin {

// Registers Interact verbs `map2d_run` (default china), `map2d_orthogrid_run`,
// and `map2d_print_run` (2D analogues of world3d / orthogrid3d showcase verbs).
void register_map2d_interact_verbs();

}  // namespace plugin

#endif  // PLUGIN_MAP2D_SCENARIO_INTERACT_H_
