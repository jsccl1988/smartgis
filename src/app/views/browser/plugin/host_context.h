// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_BROWSER_PLUGIN_HOST_CONTEXT_H_
#define APP_VIEWS_BROWSER_PLUGIN_HOST_CONTEXT_H_

#include <functional>

namespace content {
class BrowserSession;
class Map2dPresenter;
class MapScene;
class OrbitFrame;
class PluginHost;
class Scene3dPresenter;
class ViewFrame;
}  // namespace content

namespace app {
namespace detail {

// Narrow PluginHost Scene3dSink wiring. No Browser* — horizon fills callbacks.
struct Scene3dHostContext {
  content::BrowserSession* session = nullptr;
  content::MapScene* document = nullptr;
  content::Scene3dPresenter* scene3d = nullptr;
  content::OrbitFrame* orbit = nullptr;
  content::PluginHost* host = nullptr;
  std::function<void()> present_scene3d;
  std::function<void()> apply_china_product;
  std::function<void()> apply_china_atmo;
  std::function<void()> push_shared_extent;
  std::function<void(int)> select_map_tab;
};

// Narrow PluginHost Map2dSink wiring. No Browser* — horizon fills callbacks.
struct Map2dHostContext {
  content::BrowserSession* session = nullptr;
  content::MapScene* document = nullptr;
  content::Map2dPresenter* map2d = nullptr;
  content::ViewFrame* view_frame = nullptr;
  content::PluginHost* host = nullptr;
  std::function<void()> present_map2d;
  std::function<void(int, int)> apply_china_product;
  std::function<void()> push_shared_extent;
  std::function<void(int)> select_map_tab;
  std::function<void(int*, int*)> view_size;
};

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_BROWSER_PLUGIN_HOST_CONTEXT_H_
