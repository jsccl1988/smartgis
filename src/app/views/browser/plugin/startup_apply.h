// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_BROWSER_PLUGIN_STARTUP_APPLY_H_
#define APP_VIEWS_BROWSER_PLUGIN_STARTUP_APPLY_H_

#include <string>

namespace content {
class PluginHost;
}  // namespace content

namespace plugin {
class Registry;
}  // namespace plugin

namespace app {
namespace detail {

// Apply plugin.json `startup` across activated packs. Highest priority wins
// viewport / scenario; enables packs that declare commands or seed.
bool apply_registry_startup(plugin::Registry* registry,
                            content::PluginHost* host,
                            std::string* startup_viewport,
                            std::string* startup_scenario);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_BROWSER_PLUGIN_STARTUP_APPLY_H_
