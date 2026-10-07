// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/interact.h"

#include "plugin/runtime/host/capability/scenario.h"

namespace plugin {

void register_world3d_interact_ops() {
  register_scenario_mode_op(
      "atmosphere_run", "full",
      {
          {"land", "world3d.scenario.atmosphere.land"},
          {"ocean", "world3d.scenario.atmosphere.ocean"},
          {"full", "world3d.scenario.atmosphere.full"},
          {"coast", "world3d.scenario.atmosphere.coast"},
          {"globe", "world3d.scenario.atmosphere.globe"},
          {"earth", "world3d.scenario.atmosphere.globe"},
          {"legacy", "world3d.scenario.atmosphere.legacy"},
      });
  register_scenario_command_op("world3d_run", "world3d.scenario.showcase");
  register_scenario_command_op("orthogrid3d_run", "orthogrid3d.scenario.showcase");
  register_scenario_command_op("orthogrid_run", "orthogrid.scenario.showcase");
}

}  // namespace plugin
