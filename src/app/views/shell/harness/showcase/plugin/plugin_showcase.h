// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_SHOWCASE_PLUGIN_SHOWCASE_H_
#define APP_VIEWS_SHELL_SHOWCASE_PLUGIN_SHOWCASE_H_

#include <string>

namespace app {

class Browser;

// Product plugin sample+viz. |mode| is a canonical showcase id
// (normalize_plugin_showcase_id). Scene3D C++ bodies: world3d / mine /
// stormsurge / orthogrid3d. Map2d C++: orthogrid / print / traffic. Report:
// inspector dock. Remaining modes (flood / geochem) use plugin.<id>/*.il.
int run_plugin_showcase(Browser& browser, const std::string& mode);

// world3d / mine / stormsurge / orthogrid3d: GPU Scene3D + shell HWND capture.
// orthogrid / traffic / print: Map2d export_bmp (GDI overlay OK).
// report: ReportPanel + PluginHost::open_report.
int plugin_showcase_body(Browser& browser, const std::string& mode);

}  // namespace app

#endif  // APP_VIEWS_SHELL_SHOWCASE_PLUGIN_SHOWCASE_H_
