// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_STORMSURGE_H_
#define APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_STORMSURGE_H_

namespace content {
class MapScene;
class Scene3dPresenter;
}

namespace app {

class Browser;
class BrowserUiDelegate;
class AnalysisPlayback;

namespace detail {

bool commit_stormsurge_mask(content::MapScene* doc,
                            BrowserUiDelegate* ui,
                            AnalysisPlayback* session,
                            content::Scene3dPresenter* scene3d,
                            const unsigned char* mask,
                            int width,
                            int height,
                            const double* geotransform,
                            int frame_index,
                            int frame_count,
                            double water_level);

bool commit_stormsurge_water_mesh(content::MapScene* doc,
                                  BrowserUiDelegate* ui,
                                  AnalysisPlayback* session,
                                  content::Scene3dPresenter* scene3d,
                                  const double* xyz,
                                  int point_count,
                                  const int* triangles,
                                  int triangle_count,
                                  int frame_index);

void wire_stormsurge_analysis_writers(Browser* browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_STORMSURGE_H_
