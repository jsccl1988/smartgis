// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_GEOCHEM_H_
#define APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_GEOCHEM_H_

#include <string>

namespace content {
class MapScene;
}

namespace gis {
namespace detail {
struct GeochemSampleSet;
}
}

namespace plugin {
struct GeochemCommit;
}

namespace app {

class Browser;
class BrowserUiDelegate;

namespace detail {

bool commit_geochem(content::MapScene* doc,
                    BrowserUiDelegate* ui,
                    const plugin::GeochemCommit& commit,
                    std::string* err);

bool read_geochem_active_layer(content::MapScene* doc,
                               const std::string& element,
                               gis::detail::GeochemSampleSet* out,
                               std::string* err);

void wire_geochem_analysis_writers(Browser* browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITER_GEOCHEM_H_
