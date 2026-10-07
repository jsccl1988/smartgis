// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/stormsurge/scenario/interact.h"

#include "plugin/runtime/host/capability/scenario.h"

namespace plugin {

void register_stormsurge_interact_ops() {
  register_scenario_command_op("stormsurge_run", "stormsurge.scenario.showcase");
}

}  // namespace plugin
