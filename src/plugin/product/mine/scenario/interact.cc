// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/mine/scenario/interact.h"

#include "plugin/runtime/host/capability/scenario.h"

namespace plugin {

void register_mine_interact_ops() {
  register_scenario_command_op("mine_run", "mine.scenario.showcase");
}

}  // namespace plugin
