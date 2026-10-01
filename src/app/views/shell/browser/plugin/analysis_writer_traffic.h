// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_TRAFFIC_H_
#define APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_TRAFFIC_H_

namespace content {
class MapScene;
}

namespace app {

class Browser;
class BrowserUiDelegate;
class AnalysisPlayback;

namespace detail {

bool commit_traffic_path(content::MapScene* doc,
                         BrowserUiDelegate* ui,
                         AnalysisPlayback* session,
                         const double* xy,
                         int point_count,
                         double total_cost,
                         int frames,
                         const char* network_path);

void wire_traffic_analysis_writers(Browser* browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_TRAFFIC_H_
