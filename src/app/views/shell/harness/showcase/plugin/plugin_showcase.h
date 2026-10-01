// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_SHOWCASE_PLUGIN_SHOWCASE_H_
#define APP_VIEWS_SHELL_SHOWCASE_PLUGIN_SHOWCASE_H_

#include "app/views/shell/app/cmdline/views_launch_options.h"

namespace app {

class Browser;

// Product plugin sample+viz. world3d / mine / stormsurge / orthogrid3d use
// Scene3D C++ bodies; orthogrid uses Map2d; remaining modes use
// testing/tools/harness/plugin/plugin.<mode>/*.il CapabilityHost scripts.
int run_plugin_showcase(Browser& browser, PluginShowcaseMode mode);

// world3d: Scene3D DEM + pointcloud HWND capture.
// mine: Scene3D TIN + borehole sticks HWND capture.
// stormsurge: Scene3D water TIN HWND capture.
// orthogrid: Map2d processing + export_bmp.
// orthogrid3d: Scene3D hex TIN HWND capture.
// Other modes: script stub.
int plugin_showcase_body(Browser& browser, PluginShowcaseMode mode);

}  // namespace app

#endif  // APP_VIEWS_SHELL_SHOWCASE_PLUGIN_SHOWCASE_H_
