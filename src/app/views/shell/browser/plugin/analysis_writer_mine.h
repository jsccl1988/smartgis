// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_MINE_H_
#define APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_MINE_H_

#include <string>

namespace content {
class MapScene;
class Scene3dPresenter;
}

namespace gis {
namespace detail {
struct StratumTin;
struct BoreholeSet;
}
}

namespace app {

class Browser;
class BrowserUiDelegate;

namespace detail {

bool commit_mine_stratum(content::MapScene* doc,
                         BrowserUiDelegate* ui,
                         content::Scene3dPresenter* scene3d,
                         const gis::detail::StratumTin& tin,
                         const gis::detail::BoreholeSet& holes,
                         std::string* err);

void wire_mine_analysis_writers(Browser* browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_MINE_H_
