// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_ORTHOGRID_H_
#define APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_ORTHOGRID_H_

namespace content {
class MapScene;
class Scene3dPresenter;
}

namespace plugin {
struct OrthogridMeshCommit;
struct HexGridCommit;
}

namespace app {

class Browser;
class BrowserUiDelegate;

namespace detail {

bool commit_orthogrid_mesh(content::MapScene* doc,
                           BrowserUiDelegate* ui,
                           const plugin::OrthogridMeshCommit& mesh);

bool commit_hex_grid_mesh(content::MapScene* doc,
                          BrowserUiDelegate* ui,
                          content::Scene3dPresenter* scene3d,
                          const plugin::HexGridCommit& commit);

void wire_orthogrid_analysis_writers(Browser* browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_ORTHOGRID_H_
