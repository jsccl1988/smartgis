// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MINE_COMMANDS_H_
#define PLUGIN_MINE_COMMANDS_H_

#include <functional>
#include <string>

#include "gis/analysis/geology/borehole.h"
#include "gis/analysis/geology/stratum_tin.h"

namespace content {
class PluginHost;
}

namespace plugin {

// map2d / scene3d seam: stratum TIN + borehole sticks.
using MineStratumWriter =
    std::function<bool(const gis::detail::StratumTin& tin,
                       const gis::detail::BoreholeSet& holes,
                       std::string* err)>;

void set_mine_stratum_writer(MineStratumWriter writer);

bool register_mine(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_MINE_COMMANDS_H_
