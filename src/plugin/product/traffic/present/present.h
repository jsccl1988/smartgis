// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_TRAFFIC_PRESENT_PRESENT_H_
#define PLUGIN_TRAFFIC_PRESENT_PRESENT_H_

namespace content {
class GisDocument;
}

namespace plugin {

// Map2d land blocks + network + path. |anim_prefix_points| is the playback
// prefix length (product session owns the frame buffer; horizon ticks
// traffic.present_frame).
bool present_traffic_path(content::GisDocument* doc,
                          const double* xy,
                          int point_count,
                          double total_cost,
                          const char* network_path,
                          int anim_prefix_points);

}  // namespace plugin

#endif  // PLUGIN_TRAFFIC_PRESENT_PRESENT_H_
