// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_FLOOD_H_
#define APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_FLOOD_H_

namespace content {
class MapScene;
}

namespace app {

class Browser;
class BrowserUiDelegate;
class AnalysisPlayback;

namespace detail {

bool paint_flood_mask_layer(content::MapScene* doc,
                            BrowserUiDelegate* ui,
                            const unsigned char* mask,
                            int width,
                            int height,
                            const double* geotransform,
                            double water_level,
                            bool rebuild_terrain,
                            bool add_water_standin);

bool commit_flood_mask(content::MapScene* doc,
                       BrowserUiDelegate* ui,
                       AnalysisPlayback* session,
                       const unsigned char* mask,
                       int width,
                       int height,
                       const double* geotransform,
                       int frame_index,
                       int frame_count,
                       double water_level);

void wire_flood_analysis_writers(Browser* browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_FLOOD_H_
