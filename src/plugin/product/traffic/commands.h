// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_TRAFFIC_COMMANDS_H_
#define PLUGIN_TRAFFIC_COMMANDS_H_

#include <functional>
#include <string>
#include <vector>

namespace content {
class PluginHost;
}

namespace plugin {

// map2d / scene3d seam: progressive path reveal (interleaved x,y).
// network_path may be null; when set, writer draws the road graph under the path.
using TrafficPathWriter = std::function<bool(const double* xy,
                                             int point_count,
                                             double total_cost,
                                             int frames,
                                             const char* network_path)>;

void set_traffic_path_writer(TrafficPathWriter writer);

bool register_traffic(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_TRAFFIC_COMMANDS_H_
