// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PROJ_PROJ_COMMANDS_H_
#define PLUGIN_PROJ_PROJ_COMMANDS_H_

#include <vector>

namespace content {
class PluginHost;
}

namespace plugin {

bool register_proj(content::PluginHost* host);

// Filled by proj.transform_xy processing; consumed by MapPrjXyPage after Apply.
struct TransformXyOutput {
  double x = 0.0;
  double y = 0.0;
  bool valid = false;
};

TransformXyOutput consume_transform_xy_output();

// Projected grid nodes from proj.transform_grid. Empty when the run failed.
struct TransformGridOutput {
  std::vector<double> x;
  std::vector<double> y;
  int rows = 0;
  int cols = 0;
  bool valid = false;
};

TransformGridOutput consume_transform_grid_output();

}  // namespace plugin

#endif  // PLUGIN_PROJ_PROJ_COMMANDS_H_
