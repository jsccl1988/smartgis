// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID_COMMANDS_H_
#define PLUGIN_ORTHOGRID_COMMANDS_H_

#include <cstddef>

namespace content {
class PluginHost;
}

namespace plugin {

// Registers boundary input/save/load commands and orth-grid processing.
bool register_orthogrid(content::PluginHost* host);

// Digitizing arms a flag (0 or 2). The next linestring the shell notes is
// stored and save_boundary writes it as gridbnd text.
void arm_grid_boundary(int flag);
bool grid_boundary_armed();
bool note_grid_boundary(const double* xy, size_t count);

}  // namespace plugin

#endif  // PLUGIN_ORTHOGRID_COMMANDS_H_
