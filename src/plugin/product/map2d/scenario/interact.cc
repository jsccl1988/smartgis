// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/interact.h"

#include "plugin/runtime/host/capability/scenario.h"

namespace plugin {

void register_map2d_interact_ops() {
  register_scenario_mode_op("map2d_run", "china",
                            {
                                {"china", "map2d.scenario.china"},
                                {"align", "map2d.scenario.align"},
                                {"orthogrid", "map2d.scenario.orthogrid"},
                                {"print", "map2d.scenario.print"},
                            });
}

}  // namespace plugin
