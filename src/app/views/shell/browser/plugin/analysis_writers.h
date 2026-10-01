// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITERS_H_
#define APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITERS_H_

namespace app {

class Browser;

// Registers product plugin document / scene / analysis writers on |browser|.
// Call once from Browser::init after chrome UI exists.
void wire_plugin_analysis_writers(Browser* browser);

}  // namespace app

#endif  // APP_VIEWS_SHELL_BROWSER_PLUGIN_ANALYSIS_WRITERS_H_
