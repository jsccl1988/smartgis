// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_COMMON_H_
#define APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_COMMON_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace content {
class MapScene;
}

namespace app {

class BrowserUiDelegate;

namespace detail {

// Shared helpers for product analysis writers (pool-safe; no Views widgets).
bool refresh_ui_after_layer(BrowserUiDelegate* ui);

bool add_standin_mesh(content::MapScene* doc,
                      BrowserUiDelegate* ui,
                      const char* name,
                      double lon,
                      double lat,
                      double half_deg);

bool apply_style_json(content::MapScene* doc, const char* json);

bool append_map_polygon(content::MapScene* doc,
                        const std::vector<std::pair<double, double>>& ring,
                        const char* heat);

bool append_map_polyline(content::MapScene* doc,
                         const std::vector<std::pair<double, double>>& xy,
                         const char* frame_tag);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_COMMON_H_
