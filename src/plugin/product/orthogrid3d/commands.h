// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID3D_COMMANDS_H_
#define PLUGIN_ORTHOGRID3D_COMMANDS_H_

#include <functional>

#include "gis/kernel/geo/mesh/geometry.h"

namespace content {
class PluginHost;
}

namespace plugin {

// Solved volume mesh for Scene3D / analysis commit.
struct HexGridCommit {
  const geo::HexGrid* grid = nullptr;
  const float* cell_orth = nullptr;
  int cell_orth_count = 0;
};

using HexGridWriter = std::function<bool(const HexGridCommit&)>;
void set_hex_grid_writer(HexGridWriter writer);
bool publish_hex_grid(const HexGridCommit& commit);

// Registers generate + create_hex_grid processing.
bool register_orthogrid3d(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_ORTHOGRID3D_COMMANDS_H_
