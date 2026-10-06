// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_BROWSER_PLUGIN_PRESENT_H_
#define APP_VIEWS_BROWSER_PLUGIN_PRESENT_H_

#include <functional>
#include <string>
#include <string_view>

namespace content {
class MapScene;
class Map2dPresenter;
class PluginHost;
}

namespace app {

class Browser;
class BrowserUiDelegate;
class PluginPlayback;
class PluginShell;

namespace detail {

// Switch the shell Map / Scene3D tab so plugin results paint in the main view.
void present_plugin_map2d(BrowserUiDelegate* ui, content::Map2dPresenter* map2d,
                          const std::function<void()>& fit_extent);
void present_plugin_scene3d(BrowserUiDelegate* ui);

// surface 0 = main tabs; surface 1 = shared MapPreview / WorldPreview window.
bool present_plugin_dataset(Browser* browser, content::Map2dPresenter* map2d,
                            const std::function<void()>& fit_extent,
                            std::string_view path, int face, int surface);

bool add_standin_mesh(content::MapScene* doc, const char* name, double lon,
                      double lat, double half_deg);

// Processing id ending in ".present_frame" contributed by |plugin_id|.
std::string present_frame_processing_id(content::PluginHost* host,
                                        std::string_view plugin_id);

bool present_plugin_frame(PluginShell* plugins, PluginPlayback* session,
                          int index);

int export_plugin_frames(PluginShell* plugins, PluginPlayback* session,
                         content::Map2dPresenter* map2d,
                         const std::string& dir_leaf);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_BROWSER_PLUGIN_PRESENT_H_
